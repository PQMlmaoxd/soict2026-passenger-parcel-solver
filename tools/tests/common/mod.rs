//! Helpers shared by the integration tests: running the solver, temporary
//! files, and a brute force oracle that is independent of the solver.

#![allow(dead_code)] // each test binary uses a different subset

use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;
use std::time::Instant;

use solver_tools::checker::{check_files, evaluate};
use solver_tools::generator::{generate, Layout, Params};
use solver_tools::instance::{read_instance, Instance};

/// Repository root, one level above the tools crate.
pub fn repo_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("tools has a parent")
        .to_path_buf()
}

/// Solver binary: `$SOLVER`, or `build/solver` after `make build`.
pub fn solver_path() -> PathBuf {
    let path = std::env::var_os("SOLVER")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            let name = if cfg!(windows) {
                "solver.exe"
            } else {
                "solver"
            };
            repo_root().join("build").join(name)
        });
    assert!(
        path.exists(),
        "solver binary not found at {}: run `make build` first",
        path.display()
    );
    path
}

/// Sanitizer builds run several times slower, so wall-clock assertions are optional.
pub fn check_timing() -> bool {
    std::env::var("SKIP_TIMING").map_or(true, |value| value != "1")
}

/// A temporary directory removed on drop.
pub struct TempDir(PathBuf);

impl TempDir {
    pub fn new(label: &str) -> TempDir {
        static COUNTER: AtomicUsize = AtomicUsize::new(0);
        let path = std::env::temp_dir().join(format!(
            "ppsolver-{label}-{}-{}",
            std::process::id(),
            COUNTER.fetch_add(1, Ordering::SeqCst)
        ));
        fs::create_dir_all(&path).expect("create temp dir");
        TempDir(path)
    }

    pub fn path(&self) -> &Path {
        &self.0
    }

    pub fn join(&self, name: &str) -> PathBuf {
        self.0.join(name)
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

pub struct Run {
    pub code: Option<i32>,
    pub stdout: String,
    pub stderr: String,
    pub wall: f64,
}

/// One solver at a time: the solver is wall-clock driven, so concurrent runs
/// would only make the timing assertions flaky.
static SOLVER_LOCK: Mutex<()> = Mutex::new(());

/// Runs the solver with `args`, feeding it the contents of `input`.
pub fn run_solver(input: &Path, args: &[&str]) -> Run {
    let _guard = SOLVER_LOCK
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner());
    let start = Instant::now();
    let output = Command::new(solver_path())
        .args(args)
        .stdin(fs::File::open(input).expect("open input"))
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output()
        .expect("start solver");
    Run {
        code: output.status.code(),
        stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
        stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
        wall: start.elapsed().as_secs_f64(),
    }
}

/// Parameters in the order of `gen_instance`: K N M seed layout horizon window asymmetry.
pub type Case = (usize, usize, usize, u64, Layout, f64, f64, f64);

pub fn params(case: Case) -> Params {
    let (taxis, passengers, parcels, seed, layout, horizon, window, asymmetry) = case;
    Params {
        taxis,
        passengers,
        parcels,
        seed,
        layout,
        horizon,
        window,
        asymmetry,
    }
}

pub fn write_instance(dir: &TempDir, name: &str, case: Case) -> PathBuf {
    let path = dir.join(&format!("{name}.in"));
    fs::write(&path, generate(&params(case))).expect("write instance");
    path
}

/// Validates `output` with the independent checker and returns its benefit.
pub fn validate(instance: &Path, output: &str, dir: &TempDir) -> i64 {
    let solution = dir.join("solution.out");
    fs::write(&solution, output).expect("write solution");
    match check_files(instance, &solution) {
        Ok(summary) => summary.benefit,
        Err(error) => panic!("checker rejected the output: {error}\n{output}"),
    }
}

/// Orders in which one taxi can run a set of requests: passengers ride back to
/// back, a parcel is picked up before it is delivered.
struct Orderings<'a> {
    instance: &'a Instance,
    taxi: usize,
    best: Option<i64>,
}

impl Orderings<'_> {
    fn extend(
        &mut self,
        passengers: &[usize],
        parcels: &[usize],
        in_flight: &[usize],
        route: &mut Vec<usize>,
    ) {
        if passengers.is_empty() && parcels.is_empty() && in_flight.is_empty() {
            let mut plan = vec![Vec::new(); self.instance.taxis];
            plan[self.taxi] = route.clone();
            if let Ok(summary) = evaluate(self.instance, &plan) {
                self.best = Some(
                    self.best
                        .map_or(summary.benefit, |b| b.max(summary.benefit)),
                );
            }
            return;
        }
        for (position, &i) in passengers.iter().enumerate() {
            let mut rest = passengers.to_vec();
            rest.remove(position);
            let p = &self.instance.passengers[i];
            route.extend([p.pickup, p.drop]);
            self.extend(&rest, parcels, in_flight, route);
            route.truncate(route.len() - 2);
        }
        for (position, &j) in parcels.iter().enumerate() {
            let mut rest = parcels.to_vec();
            rest.remove(position);
            let mut flying = in_flight.to_vec();
            flying.push(j);
            route.push(self.instance.parcels[j].pickup);
            self.extend(passengers, &rest, &flying, route);
            route.pop();
        }
        for (position, &j) in in_flight.iter().enumerate() {
            let mut rest = in_flight.to_vec();
            rest.remove(position);
            route.push(self.instance.parcels[j].drop);
            self.extend(passengers, parcels, &rest, route);
            route.pop();
        }
    }
}

/// Best benefit over all plans, by exhaustive search. Only for a handful of requests.
///
/// Route simulation is delegated to the checker, so the oracle and the solver
/// share nothing but the contest rules.
pub fn optimum(instance_path: &Path) -> i64 {
    let instance = read_instance(instance_path).expect("readable instance");
    let n = instance.passengers.len();
    let requests = n + instance.parcels.len();

    // value[mask] = best benefit so far of the taxis handled, serving exactly `mask`.
    let mut value = vec![i64::MIN; 1 << requests];
    value[0] = 0;
    for taxi in 0..instance.taxis {
        let mut options = vec![(0usize, 0i64)];
        for mask in 1..1usize << requests {
            let members: Vec<usize> = (0..requests).filter(|r| mask >> r & 1 == 1).collect();
            let passengers: Vec<usize> = members.iter().copied().filter(|&m| m < n).collect();
            let parcels: Vec<usize> = members.iter().filter(|&&m| m >= n).map(|m| m - n).collect();
            let mut search = Orderings {
                instance: &instance,
                taxi,
                best: None,
            };
            search.extend(&passengers, &parcels, &[], &mut Vec::new());
            if let Some(benefit) = search.best.filter(|&b| b > 0) {
                options.push((mask, benefit));
            }
        }
        let mut merged = value.clone();
        for (used, &total) in value.iter().enumerate().filter(|(_, &v)| v != i64::MIN) {
            for &(mask, benefit) in &options {
                if used & mask == 0 && total + benefit > merged[used | mask] {
                    merged[used | mask] = total + benefit;
                }
            }
        }
        value = merged;
    }
    value.into_iter().max().expect("at least the empty plan")
}
