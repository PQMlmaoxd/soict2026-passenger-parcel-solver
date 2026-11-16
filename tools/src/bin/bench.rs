//! `bench --solver BASELINE [--solver VARIANT ...] --instances PATH... [options]`
//!
//! Options:
//!   --time SECONDS   passed to every solver as --time (default: the solver's own)
//!   --seeds 42,1     solver seeds, one run each per instance (default 42)
//!   --jobs N         runs in parallel (default 1; use at most one per physical core)
//!   --out FILE       write one CSV row per run
//!
//! A PATH is an instance file or a directory holding `*.in` files. The first
//! solver is the baseline for the paired comparison.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use solver_tools::bench::{run_all, summarize, to_csv, Job};

const USAGE: &str = "usage: bench --solver BASELINE [--solver VARIANT ...] --instances PATH... [--time SECONDS] [--seeds 42,1] [--jobs N] [--out FILE]";

struct Options {
    solvers: Vec<PathBuf>,
    instances: Vec<PathBuf>,
    time: Option<f64>,
    seeds: Vec<u64>,
    jobs: usize,
    out: Option<PathBuf>,
}

fn parse(args: Vec<String>) -> Result<Options, String> {
    let mut options = Options {
        solvers: vec![],
        instances: vec![],
        time: None,
        seeds: vec![42],
        jobs: 1,
        out: None,
    };
    let mut args = args.into_iter().peekable();
    while let Some(flag) = args.next() {
        let mut value = |name: &str| args.next().ok_or_else(|| format!("{name} needs a value"));
        match flag.as_str() {
            "--solver" => options.solvers.push(PathBuf::from(value("--solver")?)),
            "--time" => {
                options.time = Some(
                    value("--time")?
                        .parse()
                        .map_err(|_| "bad --time".to_string())?,
                )
            }
            "--seeds" => {
                options.seeds = value("--seeds")?
                    .split(',')
                    .map(|s| s.trim().parse().map_err(|_| format!("bad seed {s}")))
                    .collect::<Result<_, _>>()?
            }
            "--jobs" => {
                options.jobs = value("--jobs")?
                    .parse()
                    .map_err(|_| "bad --jobs".to_string())?
            }
            "--out" => options.out = Some(PathBuf::from(value("--out")?)),
            "--instances" => {
                // Everything up to the next flag is an instance path.
                while let Some(next) = args.peek() {
                    if next.starts_with("--") {
                        break;
                    }
                    options
                        .instances
                        .push(PathBuf::from(args.next().expect("peeked")));
                }
            }
            other => return Err(format!("unknown option {other}")),
        }
    }
    if options.solvers.is_empty() || options.instances.is_empty() {
        return Err("need at least one --solver and one --instances path".into());
    }
    Ok(options)
}

fn expand_instances(paths: &[PathBuf]) -> Result<Vec<PathBuf>, String> {
    let mut files = Vec::new();
    for path in paths {
        if path.is_dir() {
            let mut found: Vec<PathBuf> = fs::read_dir(path)
                .map_err(|e| format!("{}: {e}", path.display()))?
                .filter_map(|entry| entry.ok().map(|e| e.path()))
                .filter(|p| p.extension().is_some_and(|ext| ext == "in"))
                .collect();
            found.sort();
            files.extend(found);
        } else {
            files.push(path.clone());
        }
    }
    Ok(files)
}

fn solver_names(solvers: &[PathBuf]) -> Vec<String> {
    solvers
        .iter()
        .enumerate()
        .map(|(index, path)| {
            let stem = path
                .file_stem()
                .map(|s| s.to_string_lossy().into_owned())
                .unwrap_or_default();
            let clash = solvers
                .iter()
                .filter(|p| p.file_stem() == path.file_stem())
                .count()
                > 1;
            if clash {
                format!("{stem}#{index}")
            } else {
                stem
            }
        })
        .collect()
}

fn main() -> ExitCode {
    let options = match parse(std::env::args().skip(1).collect()) {
        Ok(options) => options,
        Err(message) => {
            eprintln!("{message}\n\n{USAGE}");
            return ExitCode::from(2);
        }
    };
    let instances = match expand_instances(&options.instances) {
        Ok(files) if !files.is_empty() => files,
        Ok(_) => {
            eprintln!("no instances found");
            return ExitCode::from(2);
        }
        Err(message) => {
            eprintln!("{message}");
            return ExitCode::from(2);
        }
    };

    let mut jobs = Vec::new();
    for instance in &instances {
        for &seed in &options.seeds {
            for arm in 0..options.solvers.len() {
                jobs.push(Job {
                    arm,
                    instance: instance.clone(),
                    seed,
                });
            }
        }
    }
    let scratch = std::env::temp_dir().join(format!("bench-{}", std::process::id()));
    if let Err(error) = fs::create_dir_all(&scratch) {
        eprintln!("cannot create {}: {error}", scratch.display());
        return ExitCode::from(1);
    }
    let outcomes = run_all(
        &options.solvers,
        &jobs,
        options.time,
        options.jobs,
        &scratch,
    );
    let _ = fs::remove_dir_all(&scratch);

    let names = solver_names(&options.solvers);
    if let Some(path) = &options.out {
        if let Err(error) = fs::write(path, to_csv(&names, &outcomes)) {
            eprintln!("cannot write {}: {error}", Path::new(path).display());
            return ExitCode::from(1);
        }
    }
    print!("{}", summarize(&names, &outcomes));
    if outcomes.iter().any(|o| o.benefit.is_none()) {
        eprintln!("some runs were not VALID");
        return ExitCode::from(1);
    }
    ExitCode::SUCCESS
}
