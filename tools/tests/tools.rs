//! The helper programs: generator, checker, amalgamation and the equivalence check.

mod common;

use std::fs;
use std::path::Path;
use std::process::{Command, Output};

use common::{repo_root, write_instance, TempDir};
use solver_tools::generator::{generate, Layout, Params};

fn tool(name: &str) -> Command {
    let path = match name {
        "checker" => env!("CARGO_BIN_EXE_checker"),
        "check_equivalence" => env!("CARGO_BIN_EXE_check_equivalence"),
        "amalgamate" => env!("CARGO_BIN_EXE_amalgamate"),
        "bench" => env!("CARGO_BIN_EXE_bench"),
        other => panic!("unknown tool {other}"),
    };
    Command::new(path)
}

fn text(output: &Output) -> String {
    String::from_utf8_lossy(&output.stdout).into_owned()
}

fn compiler() -> String {
    std::env::var("CXX").unwrap_or_else(|_| "g++".into())
}

fn has_compiler() -> bool {
    Command::new(compiler()).arg("--version").output().is_ok()
}

// ---- generator -----------------------------------------------------------

/// Golden files pin the generator: benchmark instances must stay byte-identical
/// across releases, and these three match the Python generator the instances
/// were first made with.
#[test]
fn generator_output_is_pinned() {
    let golden = repo_root().join("tools/tests/golden");
    let cases = [
        ("uni_k3_n2_m2_s4.in", Layout::Uniform, 4, 50.0, 2.0, 0.0),
        ("clu_k3_n2_m2_s7.in", Layout::Clustered, 7, 60.0, 3.0, 0.25),
        ("hub_k3_n2_m2_s9.in", Layout::Hub, 9, 60.0, 3.0, 0.5),
    ];
    for (file, layout, seed, horizon, window, asymmetry) in cases {
        let params = Params {
            taxis: 3,
            passengers: 2,
            parcels: 2,
            seed,
            layout,
            horizon,
            window,
            asymmetry,
        };
        let expected = fs::read_to_string(golden.join(file)).unwrap();
        assert_eq!(generate(&params), expected, "{file}");
    }
}

#[test]
fn generator_dimensions() {
    let params = Params {
        taxis: 3,
        passengers: 4,
        parcels: 5,
        seed: 1,
        layout: Layout::Hub,
        horizon: 100.0,
        window: 5.0,
        asymmetry: 0.3,
    };
    let text = generate(&params);
    let vertices = 2 * 9 + 3;
    assert_eq!(text.lines().next(), Some("3 4 5"));
    assert_eq!(text.lines().count(), 1 + 3 + 9 + 2 * vertices);
}

// ---- checker -------------------------------------------------------------

fn check(solution: &str) -> (Option<i32>, String) {
    let dir = TempDir::new("checker");
    let instance = dir.join("case.in");
    let params = Params {
        taxis: 2,
        passengers: 2,
        parcels: 1,
        seed: 3,
        layout: Layout::Uniform,
        horizon: 40.0,
        window: 3.0,
        asymmetry: 0.2,
    };
    fs::write(&instance, generate(&params)).unwrap();
    let path = dir.join("case.out");
    fs::write(&path, solution).unwrap();
    let output = tool("checker").arg(&instance).arg(&path).output().unwrap();
    (output.status.code(), text(&output))
}

#[test]
fn checker_accepts_the_idle_plan() {
    let (code, out) = check("0\n0\n0\n");
    assert_eq!(code, Some(0), "{out}");
    assert!(out.starts_with("VALID benefit=0"), "{out}");
}

#[test]
fn checker_rejects_rule_violations() {
    // Vertices 3 and 4 are the pickup and drop-off of passenger 0.
    for (label, solution) in [
        ("wrong declared benefit", "5\n0\n0\n"),
        ("missing route line", "0\n0\n"),
        ("unknown vertex", "0\n1 99\n0\n"),
        ("passenger served by pickup only", "0\n1 3\n0\n"),
        ("non-integer token", "0\n1 x\n0\n"),
    ] {
        let (code, out) = check(solution);
        assert_eq!(code, Some(1), "{label}: {out}");
        assert!(out.starts_with("INVALID"), "{label}: {out}");
    }
}

#[test]
fn checker_reports_missing_files_with_exit_code_two() {
    let output = tool("checker")
        .args(["no-such.in", "no-such.out"])
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(2));
}

// ---- amalgamation and equivalence ----------------------------------------

fn run_equivalence(source: &Path, extra: &[&str]) -> Output {
    tool("check_equivalence")
        .current_dir(repo_root())
        .args(["--cxx", &compiler(), "--source"])
        .arg(source)
        .args(extra)
        .output()
        .unwrap()
}

#[test]
fn modular_sources_are_the_submitted_program() {
    if !has_compiler() {
        eprintln!("skipped: no C++ compiler");
        return;
    }
    let output = run_equivalence(Path::new("src/main.cpp"), &[]);
    assert_eq!(output.status.code(), Some(0), "{}", text(&output));
}

#[test]
fn amalgamated_file_is_the_submitted_program() {
    if !has_compiler() {
        eprintln!("skipped: no C++ compiler");
        return;
    }
    let dir = TempDir::new("amalgamate");
    let merged = dir.join("solver.cpp");
    let status = tool("amalgamate")
        .current_dir(repo_root())
        .args(["--output"])
        .arg(&merged)
        .status()
        .unwrap();
    assert!(status.success());
    let body = fs::read_to_string(&merged).unwrap();
    assert!(!body.contains("#pragma once"));
    assert!(!body.contains("#include \""));
    let output = run_equivalence(&merged, &[]);
    assert_eq!(output.status.code(), Some(0), "{}", text(&output));
}

#[test]
fn equivalence_check_notices_a_changed_constant() {
    if !has_compiler() {
        eprintln!("skipped: no C++ compiler");
        return;
    }
    let dir = TempDir::new("mutation");
    let copy = dir.join("src");
    fs::create_dir_all(&copy).unwrap();
    for entry in fs::read_dir(repo_root().join("src")).unwrap() {
        let path = entry.unwrap().path();
        fs::copy(&path, copy.join(path.file_name().unwrap())).unwrap();
    }
    let module = copy.join("solver.hpp");
    let original = fs::read_to_string(&module).unwrap();
    let constant = "envd(\"SISR_START\", 0.25)";
    assert!(
        original.contains(constant),
        "{constant} moved out of solver.hpp"
    );
    fs::write(
        &module,
        original.replace(constant, "envd(\"SISR_START\", 0.26)"),
    )
    .unwrap();

    let output = run_equivalence(&copy.join("main.cpp"), &[]);
    assert_eq!(output.status.code(), Some(1), "{}", text(&output));
    assert!(text(&output).contains("DIFFERENT"));
}

#[test]
fn comments_and_layout_do_not_count_as_changes() {
    if !has_compiler() {
        eprintln!("skipped: no C++ compiler");
        return;
    }
    let dir = TempDir::new("layout");
    let copy = dir.join("src");
    fs::create_dir_all(&copy).unwrap();
    for entry in fs::read_dir(repo_root().join("src")).unwrap() {
        let path = entry.unwrap().path();
        fs::copy(&path, copy.join(path.file_name().unwrap())).unwrap();
    }
    let module = copy.join("types.hpp");
    let original = fs::read_to_string(&module).unwrap();
    fs::write(&module, format!("{original}\n// A comment.\n\n\n")).unwrap();
    let output = run_equivalence(&copy.join("main.cpp"), &[]);
    assert_eq!(output.status.code(), Some(0), "{}", text(&output));
}

// ---- benchmark runner ----------------------------------------------------

#[test]
fn bench_runs_a_paired_comparison_and_writes_csv() {
    let dir = TempDir::new("bench");
    let case = (2, 2, 2, 1, Layout::Uniform, 40.0, 2.0, 0.0);
    let instance = write_instance(&dir, "tiny", case);
    let solver = common::solver_path();
    let csv = dir.join("results.csv");
    // The same binary twice: the exact method makes both runs optimal, so no pair differs.
    let output = tool("bench")
        .arg("--solver")
        .arg(&solver)
        .arg("--solver")
        .arg(&solver)
        .arg("--instances")
        .arg(&instance)
        .args(["--time", "2", "--seeds", "1,2", "--out"])
        .arg(&csv)
        .output()
        .unwrap();
    let summary = text(&output);
    assert_eq!(output.status.code(), Some(0), "{summary}");
    assert!(summary.contains("2 of 2 runs valid"), "{summary}");
    assert!(summary.contains("mean +0.000%"), "{summary}");
    let rows = fs::read_to_string(&csv).unwrap();
    assert_eq!(rows.lines().count(), 1 + 4);
    assert!(rows.starts_with("arm,instance,seed,valid,benefit,wall_seconds,detail"));
}
