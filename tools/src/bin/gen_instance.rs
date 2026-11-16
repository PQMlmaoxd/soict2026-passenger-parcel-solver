//! `gen_instance K N M SEED LAYOUT HORIZON WINDOW ASYMMETRY > instance.in`
//!
//! LAYOUT is one of `uni`, `clu`, `hub`. Example:
//! `gen_instance 60 220 218 900001 uni 600 9 0.25 > bench.in`

use std::io::Write;
use std::process::ExitCode;

use solver_tools::generator::{generate, Layout, Params};

const USAGE: &str =
    "usage: gen_instance K N M SEED LAYOUT HORIZON WINDOW ASYMMETRY\n  LAYOUT: uni | clu | hub";

fn parse(args: &[String]) -> Result<Params, String> {
    if args.len() != 8 {
        return Err(USAGE.into());
    }
    let number = |index: usize, name: &str| -> Result<f64, String> {
        args[index]
            .parse::<f64>()
            .map_err(|_| format!("{name} must be a number"))
    };
    let count = |index: usize, name: &str| -> Result<usize, String> {
        args[index]
            .parse::<usize>()
            .map_err(|_| format!("{name} must be a non-negative integer"))
    };
    Ok(Params {
        taxis: count(0, "K")?,
        passengers: count(1, "N")?,
        parcels: count(2, "M")?,
        seed: args[3]
            .parse::<u64>()
            .map_err(|_| "SEED must be a non-negative integer".to_string())?,
        layout: Layout::parse(&args[4]).ok_or_else(|| format!("unknown layout {}", args[4]))?,
        horizon: number(5, "HORIZON")?,
        window: number(6, "WINDOW")?,
        asymmetry: number(7, "ASYMMETRY")?,
    })
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let params = match parse(&args) {
        Ok(params) => params,
        Err(message) => {
            eprintln!("{message}");
            return ExitCode::from(2);
        }
    };
    let text = generate(&params);
    if std::io::stdout().write_all(text.as_bytes()).is_err() {
        return ExitCode::from(1);
    }
    ExitCode::SUCCESS
}
