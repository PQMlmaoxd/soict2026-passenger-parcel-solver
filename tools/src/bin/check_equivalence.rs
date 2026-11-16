//! `check_equivalence [--cxx g++] [--reference FILE] [--source FILE] [--asm]`
//!
//! Proves that the modular sources are the program that was submitted. The
//! default compares the preprocessed token streams of `contest/` and
//! `src/main.cpp`. With `--asm` it also compares the generated assembly, which
//! only makes sense with the same compiler on both sides and is the strongest
//! available evidence that the two builds behave alike.

use std::path::PathBuf;
use std::process::ExitCode;

use solver_tools::source::{assembly, excerpt, first_difference, token_stream};

const DEFAULT_REFERENCE: &str = "contest/SOICT2026_Round2_S4T2A8_SISR_v3_pkasg.cpp";

fn compare(label: &str, reference: &str, source: &str) -> bool {
    match first_difference(reference, source) {
        None => {
            println!(
                "{label}: identical ({} characters)",
                reference.chars().count()
            );
            true
        }
        Some(at) => {
            println!("{label}: DIFFERENT at offset {at}");
            println!("  reference: ...{}", excerpt(reference, at));
            println!("  source:    ...{}", excerpt(source, at));
            false
        }
    }
}

fn main() -> ExitCode {
    let mut compiler = "g++".to_string();
    let mut reference = PathBuf::from(DEFAULT_REFERENCE);
    let mut source = PathBuf::from("src/main.cpp");
    let mut with_assembly = false;
    let mut args = std::env::args().skip(1);
    while let Some(flag) = args.next() {
        match flag.as_str() {
            "--asm" => with_assembly = true,
            "--cxx" | "--reference" | "--source" => {
                let Some(value) = args.next() else {
                    eprintln!("{flag} needs a value");
                    return ExitCode::from(2);
                };
                match flag.as_str() {
                    "--cxx" => compiler = value,
                    "--reference" => reference = PathBuf::from(value),
                    _ => source = PathBuf::from(value),
                }
            }
            _ => {
                eprintln!("usage: check_equivalence [--cxx g++] [--reference FILE] [--source FILE] [--asm]");
                return ExitCode::from(2);
            }
        }
    }

    let tokens = |file: &PathBuf| token_stream(&compiler, file);
    let (left, right) = match (tokens(&reference), tokens(&source)) {
        (Ok(left), Ok(right)) => (left, right),
        (Err(error), _) | (_, Err(error)) => {
            eprintln!("preprocessing failed: {error}");
            return ExitCode::from(2);
        }
    };
    let mut same = compare("token streams", &left, &right);

    if with_assembly {
        let result = assembly(&compiler, &reference)
            .and_then(|a| assembly(&compiler, &source).map(|b| (a, b)));
        match result {
            Ok((a, b)) => same &= compare("assembly (-O2)", &a, &b),
            Err(error) => {
                eprintln!("compilation failed: {error}");
                return ExitCode::from(2);
            }
        }
    }
    if same {
        ExitCode::SUCCESS
    } else {
        ExitCode::from(1)
    }
}
