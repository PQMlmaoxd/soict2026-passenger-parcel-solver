//! Behaviour of the solver binary: optimality on tiny instances, validity, command line.
//!
//! These tests need the solver: run `make build` first, or point `SOLVER` at a binary.

mod common;

use common::{check_timing, optimum, run_solver, validate, write_instance, Case, TempDir};
use solver_tools::generator::Layout::{Clustered, Hub, Uniform};

/// K, N, M, seed, layout, horizon, window, asymmetry. Small horizons and window
/// scales make time windows bite, so not every request can be combined and the
/// optimum is not simply "serve everything".
const TINY_CASES: [Case; 10] = [
    (2, 2, 2, 1, Uniform, 40.0, 2.0, 0.0),
    (2, 3, 1, 2, Uniform, 60.0, 3.0, 0.25),
    (3, 2, 2, 3, Clustered, 30.0, 2.0, 0.25),
    (2, 1, 3, 4, Hub, 50.0, 3.0, 0.5),
    (1, 3, 2, 5, Uniform, 80.0, 4.0, 0.25),
    (2, 2, 3, 6, Clustered, 25.0, 1.0, 0.5),
    (3, 3, 2, 7, Hub, 45.0, 2.0, 0.0),
    (2, 4, 0, 8, Uniform, 30.0, 2.0, 0.25),
    (2, 0, 4, 9, Uniform, 60.0, 4.0, 0.25),
    (1, 2, 3, 10, Clustered, 70.0, 5.0, 0.5),
];

const MEDIUM: Case = (8, 40, 40, 5, Uniform, 600.0, 9.0, 0.25);

fn name(case: Case) -> String {
    format!("tiny_k{}_n{}_m{}_s{}", case.0, case.1, case.2, case.3)
}

#[test]
fn exact_method_matches_brute_force() {
    // With R <= 10 and K <= 12 the solver proves optimality, so it must equal brute force.
    let dir = TempDir::new("exact");
    for case in TINY_CASES {
        let instance = write_instance(&dir, &name(case), case);
        let run = run_solver(&instance, &["--time", "5"]);
        assert_eq!(run.code, Some(0), "{}: {}", name(case), run.stderr);
        let score = validate(&instance, &run.stdout, &dir);
        assert_eq!(score, optimum(&instance), "{}", name(case));
    }
}

#[test]
fn heuristic_phases_reach_the_optimum_on_tiny_instances() {
    // --no-exact switches to ALNS and SISR. Four to five requests are easy for them.
    let dir = TempDir::new("heuristic");
    for case in &TINY_CASES[..5] {
        let instance = write_instance(&dir, &name(*case), *case);
        let run = run_solver(&instance, &["--no-exact", "--time", "3"]);
        assert_eq!(run.code, Some(0), "{}: {}", name(*case), run.stderr);
        let score = validate(&instance, &run.stdout, &dir);
        assert_eq!(score, optimum(&instance), "{}", name(*case));
    }
}

#[test]
fn medium_instance_is_valid_within_the_time_limit() {
    let dir = TempDir::new("medium");
    let instance = write_instance(&dir, "medium", MEDIUM);
    let run = run_solver(&instance, &["--time", "6"]);
    assert_eq!(run.code, Some(0), "{}", run.stderr);
    assert!(validate(&instance, &run.stdout, &dir) > 0);
    if check_timing() {
        assert!(run.wall < 7.0, "took {:.2}s for a 6s limit", run.wall);
    }
}

#[test]
fn sisr_phase_runs_and_reports_on_stderr() {
    let dir = TempDir::new("sisr");
    let instance = write_instance(&dir, "medium", MEDIUM);
    let run = run_solver(&instance, &["--time", "6"]);
    assert!(run.stderr.contains("event=sisr_cfg"), "{}", run.stderr);
    assert!(run.stderr.contains("event=sisr_exit"), "{}", run.stderr);
}

#[test]
fn rejects_bad_arguments() {
    let dir = TempDir::new("args");
    let instance = write_instance(&dir, "tiny", TINY_CASES[0]);
    for args in [
        vec!["--bogus"],
        vec!["--time"],
        vec!["--time", "abc"],
        vec!["--time", "-1"],
        vec!["--time", "299"],
    ] {
        assert_eq!(run_solver(&instance, &args).code, Some(1), "{args:?}");
    }
}

#[test]
fn rejects_malformed_input_without_printing() {
    let dir = TempDir::new("malformed");
    for (index, text) in ["", "1 0", "1 0 0\n1 10\n0\n", "x y z"].iter().enumerate() {
        let path = dir.join(&format!("bad{index}.in"));
        std::fs::write(&path, text).unwrap();
        let run = run_solver(&path, &["--time", "1"]);
        assert_eq!(run.code, Some(1), "{text:?}");
        assert!(run.stdout.is_empty(), "{text:?}");
    }
}

#[test]
fn zero_time_still_prints_a_valid_plan() {
    let dir = TempDir::new("zero");
    let instance = write_instance(&dir, "tiny", TINY_CASES[0]);
    let run = run_solver(&instance, &["--time", "0"]);
    assert_eq!(run.code, Some(0));
    assert_eq!(validate(&instance, &run.stdout, &dir), 0);
}

#[test]
fn instance_without_requests() {
    let dir = TempDir::new("empty");
    let instance = dir.join("empty.in");
    std::fs::write(&instance, "1 0 0\n1 10\n0\n0\n").unwrap();
    let run = run_solver(&instance, &["--time", "1"]);
    assert_eq!(run.code, Some(0));
    assert_eq!(
        run.stdout.split_whitespace().collect::<Vec<_>>(),
        ["0", "0"]
    );
    assert_eq!(validate(&instance, &run.stdout, &dir), 0);
}
