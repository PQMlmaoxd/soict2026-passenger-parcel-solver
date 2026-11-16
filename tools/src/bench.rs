//! Paired benchmarking of solver builds.
//!
//! Every (solver, instance, seed) triple runs once, its output goes through the
//! independent checker, and results are compared per instance against the first
//! solver, the baseline. A run that is not VALID never counts as a score.

use std::fs::{self, File};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;
use std::time::Instant;

use crate::checker::check_files;

#[derive(Debug, Clone)]
pub struct Job {
    pub arm: usize,
    pub instance: PathBuf,
    pub seed: u64,
}

#[derive(Debug, Clone)]
pub struct Outcome {
    pub arm: usize,
    pub instance: String,
    pub seed: u64,
    /// Benefit when the checker accepted the output.
    pub benefit: Option<i64>,
    pub wall_seconds: f64,
    pub detail: String,
}

/// Runs one job. `time_limit` is passed as `--time` when present.
pub fn run_job(solver: &Path, job: &Job, time_limit: Option<f64>, scratch: &Path) -> Outcome {
    let name = job
        .instance
        .file_stem()
        .map(|s| s.to_string_lossy().into_owned())
        .unwrap_or_default();
    let output = scratch.join(format!("{}-{}-{}.out", job.arm, name, job.seed));
    let mut outcome = Outcome {
        arm: job.arm,
        instance: name,
        seed: job.seed,
        benefit: None,
        wall_seconds: 0.0,
        detail: String::new(),
    };

    let mut command = Command::new(solver);
    if let Some(seconds) = time_limit {
        command.args(["--time", &seconds.to_string()]);
    }
    command.args(["--seed", &job.seed.to_string()]);
    let (stdin, stdout) = match (File::open(&job.instance), File::create(&output)) {
        (Ok(stdin), Ok(stdout)) => (stdin, stdout),
        (Err(error), _) | (_, Err(error)) => {
            outcome.detail = format!("io error: {error}");
            return outcome;
        }
    };
    let start = Instant::now();
    let status = command
        .stdin(stdin)
        .stdout(stdout)
        .stderr(Stdio::null())
        .status();
    outcome.wall_seconds = start.elapsed().as_secs_f64();
    match status {
        Ok(status) if status.success() => match check_files(&job.instance, &output) {
            Ok(summary) => {
                outcome.benefit = Some(summary.benefit);
                outcome.detail = "VALID".into();
            }
            Err(error) => outcome.detail = format!("INVALID {error}"),
        },
        Ok(status) => outcome.detail = format!("solver exited with {status}"),
        Err(error) => outcome.detail = format!("cannot start solver: {error}"),
    }
    let _ = fs::remove_file(&output);
    outcome
}

/// Runs all jobs on `threads` workers and returns the outcomes in job order.
pub fn run_all(
    solvers: &[PathBuf],
    jobs: &[Job],
    time_limit: Option<f64>,
    threads: usize,
    scratch: &Path,
) -> Vec<Outcome> {
    let next = AtomicUsize::new(0);
    let results = Mutex::new(Vec::with_capacity(jobs.len()));
    std::thread::scope(|scope| {
        for _ in 0..threads.max(1) {
            scope.spawn(|| loop {
                let index = next.fetch_add(1, Ordering::SeqCst);
                let Some(job) = jobs.get(index) else { break };
                let outcome = run_job(&solvers[job.arm], job, time_limit, scratch);
                eprintln!(
                    "[{}/{}] {} {} seed {}: {} {:.1}s",
                    index + 1,
                    jobs.len(),
                    solvers[job.arm].display(),
                    outcome.instance,
                    outcome.seed,
                    outcome
                        .benefit
                        .map_or_else(|| outcome.detail.clone(), |b| b.to_string()),
                    outcome.wall_seconds
                );
                results.lock().expect("results lock").push((index, outcome));
            });
        }
    });
    let mut results = results.into_inner().expect("results lock");
    results.sort_by_key(|(index, _)| *index);
    results.into_iter().map(|(_, outcome)| outcome).collect()
}

/// One CSV row per run.
pub fn to_csv(names: &[String], outcomes: &[Outcome]) -> String {
    let mut text = String::from("arm,instance,seed,valid,benefit,wall_seconds,detail\n");
    for outcome in outcomes {
        text.push_str(&format!(
            "{},{},{},{},{},{:.2},{}\n",
            names[outcome.arm],
            outcome.instance,
            outcome.seed,
            outcome.benefit.is_some(),
            outcome.benefit.map_or(String::new(), |b| b.to_string()),
            outcome.wall_seconds,
            outcome.detail.replace(',', ";")
        ));
    }
    text
}

/// Mean score per solver and, for every other solver, the paired difference to the first.
pub fn summarize(names: &[String], outcomes: &[Outcome]) -> String {
    let score = |arm: usize, instance: &str, seed: u64| {
        outcomes
            .iter()
            .find(|o| o.arm == arm && o.instance == instance && o.seed == seed)
            .and_then(|o| o.benefit)
    };
    let mut text = String::new();
    for (arm, name) in names.iter().enumerate() {
        let runs: Vec<&Outcome> = outcomes.iter().filter(|o| o.arm == arm).collect();
        let valid: Vec<i64> = runs.iter().filter_map(|o| o.benefit).collect();
        let mean = valid.iter().sum::<i64>() as f64 / valid.len().max(1) as f64;
        let slowest = runs.iter().map(|o| o.wall_seconds).fold(0.0, f64::max);
        text.push_str(&format!(
            "{name}: {} of {} runs valid, mean benefit {mean:.1}, slowest {slowest:.2}s\n",
            valid.len(),
            runs.len()
        ));
    }
    for arm in 1..names.len() {
        let mut deltas = Vec::new();
        for base in outcomes.iter().filter(|o| o.arm == 0) {
            if let (Some(a), Some(b)) = (base.benefit, score(arm, &base.instance, base.seed)) {
                if a != 0 {
                    deltas.push(100.0 * (b - a) as f64 / a.abs() as f64);
                }
            }
        }
        if deltas.is_empty() {
            text.push_str(&format!(
                "{} vs {}: no comparable pairs\n",
                names[arm], names[0]
            ));
            continue;
        }
        let mean = deltas.iter().sum::<f64>() / deltas.len() as f64;
        let wins = deltas.iter().filter(|d| **d > 0.0).count();
        let losses = deltas.iter().filter(|d| **d < 0.0).count();
        let worst = deltas.iter().cloned().fold(f64::INFINITY, f64::min);
        text.push_str(&format!(
            "{} vs {}: mean {mean:+.3}%, {wins} wins, {losses} losses, {} ties, worst {worst:+.2}% over {} pairs\n",
            names[arm],
            names[0],
            deltas.len() - wins - losses,
            deltas.len()
        ));
    }
    text
}
