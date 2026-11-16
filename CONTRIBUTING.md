# Contributing

Open an issue before starting a larger change, so the scope can be agreed first. Small fixes to documentation, tools and tests can go straight to a pull request.

## Setup

You need a C++17 compiler (g++ 13 or newer is what CI treats as blocking), a Rust toolchain with `rustfmt` and `clippy`, and GNU make. clang-format 19 is needed for the formatting check, the project is formatted with 19.1.

```console
make build tools     # solver and Rust tools
make test            # solver tests, about 30 seconds
make ci              # everything CI runs on every pull request
```

On Windows without a POSIX shell, run the underlying commands from the Makefile by hand. The tests find the solver through `SOLVER=<path>` or `build/solver`.

## What CI checks

| Target | Checks |
|---|---|
| `make format-check` | clang-format on `src/`, rustfmt on `tools/` |
| `make lint` | clippy with warnings as errors |
| `make test` | Solver against a brute-force optimum, validity of outputs, command line, tools |
| `make equivalence` | `src/` and the amalgamated file are the program in `contest/` |
| `make contest-check` | Hash, size (below 500,000 characters) and LF-only encoding of the submitted file |
| `make sanitize` | AddressSanitizer and UBSan runs, undefined behavior is fatal |

The workflows only call these targets. If a check fails on GitHub, `make <target>` reproduces it locally.

## Changing the solver

`make equivalence` fails on any change that is more than layout or comments. That is intended: `contest/` records what was submitted. A real change to the algorithm is a new contest build, and the steps are in [docs/architecture.md](docs/architecture.md#changing-the-program). A pull request that changes behavior needs:

- a test that fails without the change, or a measured comparison against the previous build,
- the comparison scoped as in [docs/benchmarks.md](docs/benchmarks.md): paired runs at the full time limit, on instances that were not used to choose the change, with the independent checker passing on every output,
- no new warning with `-Wall -Wextra`, and a clean sanitizer run.

Do not weaken the checker, skip tests or change a benchmark criterion to make a candidate pass.

## Style

- C++: `make format` applies the style. Comments explain why, not what. New modules go into `src/` in dependency order and are listed in `src/main.cpp`.
- Rust: `cargo fmt` and clippy. The tools crate has no dependencies, keep it that way unless the gain is large.
- Documentation: plain sentences, sentence-case headings, numbers with their scope.

## Pull requests

Describe what changed and how you tested it. If generative AI tools wrote part of the code, say so in the description. By contributing you agree that your work is released under the MIT license of this repository.
