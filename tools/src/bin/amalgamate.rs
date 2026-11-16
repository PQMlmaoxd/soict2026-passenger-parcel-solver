//! `amalgamate [--entry src/main.cpp] [--output build/solver.cpp]`
//!
//! Writes the single-file version of the solver, the form the contest accepts.

use std::fs;
use std::path::PathBuf;
use std::process::ExitCode;

use solver_tools::source::amalgamate;

fn main() -> ExitCode {
    let mut entry = PathBuf::from("src/main.cpp");
    let mut output = PathBuf::from("build/solver.cpp");
    let mut args = std::env::args().skip(1);
    while let Some(flag) = args.next() {
        let value = args.next().map(PathBuf::from);
        match (flag.as_str(), value) {
            ("--entry", Some(path)) => entry = path,
            ("--output" | "-o", Some(path)) => output = path,
            _ => {
                eprintln!("usage: amalgamate [--entry src/main.cpp] [--output build/solver.cpp]");
                return ExitCode::from(2);
            }
        }
    }
    let text = match amalgamate(&entry) {
        Ok(text) => text,
        Err(error) => {
            eprintln!("cannot read {}: {error}", entry.display());
            return ExitCode::from(1);
        }
    };
    if let Some(parent) = output.parent() {
        if let Err(error) = fs::create_dir_all(parent) {
            eprintln!("cannot create {}: {error}", parent.display());
            return ExitCode::from(1);
        }
    }
    if let Err(error) = fs::write(&output, &text) {
        eprintln!("cannot write {}: {error}", output.display());
        return ExitCode::from(1);
    }
    println!(
        "wrote {} ({} characters)",
        output.display(),
        text.chars().count()
    );
    ExitCode::SUCCESS
}
