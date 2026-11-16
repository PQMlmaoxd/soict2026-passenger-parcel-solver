//! Tools around the passenger and parcel solver.
//!
//! The solver itself is C++ (see `src/` at the repository root). Everything
//! that surrounds it is Rust and lives here:
//!
//! * [`instance`] and [`checker`]: an independent reader and simulator of the
//!   contest rules. It shares no code with the solver.
//! * [`generator`] and [`mt`]: synthetic instances, reproducible byte for byte.
//! * [`source`]: merging `src/` into the single contest file and proving that
//!   it is the program that was submitted.
//! * [`bench`]: running solver variants side by side and comparing them.

pub mod bench;
pub mod checker;
pub mod generator;
pub mod instance;
pub mod mt;
pub mod source;
