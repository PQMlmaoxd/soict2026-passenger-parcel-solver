//! `checker INSTANCE SOLUTION`
//!
//! Exit code: 0 valid, 1 invalid solution, 2 unreadable input or bad usage.

use std::path::Path;
use std::process::ExitCode;

use solver_tools::checker::check_files;
use solver_tools::instance::Failure;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: checker INSTANCE SOLUTION");
        return ExitCode::from(2);
    }
    match check_files(Path::new(&args[1]), Path::new(&args[2])) {
        Ok(summary) => {
            println!(
                "VALID benefit={} revenue={} travel_cost={} passengers={} parcels={}",
                summary.benefit,
                summary.revenue,
                summary.travel_cost,
                summary.passengers,
                summary.parcels
            );
            ExitCode::SUCCESS
        }
        Err(Failure::Invalid(message)) => {
            println!("INVALID {message}");
            ExitCode::from(1)
        }
        Err(Failure::Unreadable(message)) => {
            eprintln!("ERROR {message}");
            ExitCode::from(2)
        }
    }
}
