# Tools

Rust, standard library only. Build with `make tools` or `cargo build --release` in this directory. Binaries land in `target/release/`.

| Binary | Usage |
|---|---|
| `checker` | `checker INSTANCE SOLUTION`. Prints `VALID benefit=...` or `INVALID reason`. Exit code 0, 1, 2 for valid, invalid, unreadable. |
| `gen_instance` | `gen_instance K N M SEED LAYOUT HORIZON WINDOW ASYMMETRY > file`. Layouts `uni`, `clu`, `hub`. Same arguments give the same bytes. |
| `amalgamate` | `amalgamate [--entry src/main.cpp] [--output build/solver.cpp]`. Writes the single contest file. |
| `check_equivalence` | `check_equivalence [--cxx g++] [--source FILE] [--asm]`. Compares a source with `contest/` by token stream, and with `--asm` by assembly. |
| `bench` | `bench --solver A [--solver B] --instances DIR_OR_FILES --time S --seeds 42,1 --jobs N --out FILE`. Paired runs, every output checked, first solver is the baseline. |

`cargo test` runs the integration tests in `tests/`. They need the solver, so use `make test`, or set `SOLVER` to a binary. `tests/golden/` holds small instances that pin the generator output.
