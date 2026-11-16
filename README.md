# Passenger and parcel solver for SOICT 2026 Round 2

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](src/main.cpp)
[![Rust 1.77+](https://img.shields.io/badge/Rust-1.77%2B-orange.svg)](tools/Cargo.toml)

A single-threaded C++17 solver for the Integrated Passenger and Parcel Transportation problem of the SOICT 2026 Round 2 contest. Taxis serve passengers who must travel directly to their drop-off, and may carry parcels between those trips. Every request is optional. The objective is revenue of the served requests minus the travel cost actually driven.

The solver combines exact enumeration for small and mid-sized instances, adaptive large neighborhood search (ALNS), and a long phase of string removals (SISR) with route recombination and exact reassignment of routes to taxis. The contest takes one source file, so this repository keeps the submitted file untouched next to a modular, formatted version of the same program, and a check that proves the two are identical.

## Tech stack

| Area | Choice | Notes |
|---|---|---|
| Solver | C++17, standard library only | One thread, no external libraries. Built with g++ 14.2 on Windows and g++ 13.3 on Ubuntu 24.04. |
| Tools | Rust 2021, standard library only, 1.77 or newer | Checker, instance generator, amalgamation, equivalence check, benchmark runner, integration tests. |
| Algorithms | Exact enumeration with set packing, ALNS, SISR, Hungarian assignment | See [docs/algorithm.md](docs/algorithm.md). |
| Build | GNU make | The CI workflows call the same targets. |
| Quality | clang-format 19, rustfmt, clippy, AddressSanitizer and UBSan | Warnings are errors with g++ 13 and 14, clippy runs with `-D warnings`. |
| CI | GitHub Actions on Ubuntu 24.04 | Actions pinned by commit SHA, compiler matrix (g++ 12 and clang++ 18 are experimental), tagged releases. |

## Result

The submitted file finished 6th when the ranking closed at 09:00 ICT on 16/11/2026. It was 3rd on the public CMS scoreboard of 15/11/2026, 21:32 ICT, before other teams made their last submissions.

| Scope | Fleet baseline | This solver | Difference |
|---|---|---|---|
| Official CMS score, 39 tests | 503,071 | 512,260 | +9,189 |
| Same gain over the 8 tests that run to the deadline | 265,735 | 274,924 | +3.46% |

For Fleet, 31 of the 39 tests finish early (exact or saturated search, under 92 seconds) and scored identically in two Fleet-derived submissions, so only 8 tests can move the total. Expressing the gain over those 8 assumes the other 31 are unchanged for this solver. Its per-test scores were not recorded. Local results on generated instances are in [docs/benchmarks.md](docs/benchmarks.md), with their scope labelled. They do not predict any contest score.

## Scope

This solver targets the contest instances: up to 250 passengers and 250 parcels, 295 seconds of wall time, one thread, no external libraries, source below 500,000 characters. It handles asymmetric and nonmetric travel matrices, and it never assumes that deleting a stop keeps a route feasible.

It is not a general VRP library. Its parameters were tuned on generated instances that imitate the contest tests, and nothing here has been tested above the contest limits.

## Build and run

You need a C++17 compiler. GNU make is optional, the build is one command.

```console
make build                                   # build/solver
g++ -std=c++17 -O2 -Isrc src/main.cpp -o solver   # the same without make
./build/solver < instance.in > solution.out
```

```text
solver [--time SECONDS] [--seed N] [--no-exact] < instance > solution
```

| Option | Default | Meaning |
|---|---|---|
| `--time` | 295 | Wall-clock limit in seconds, 0 to 298. The solver stops about `min(0.15, 2%)` of that earlier. |
| `--seed` | 42 | Seed of the random generator. Runs are not bit-reproducible because phases are timed by the wall clock. |
| `--no-exact` | off | Skip the exact methods and go straight to ALNS and SISR. Useful for testing the heuristics on tiny instances. |

The solver exits with code 1 on bad arguments or malformed input and prints nothing to stdout in that case. Progress goes to stderr as `event=...` lines. The formal model and the input and output formats are in [docs/problem.md](docs/problem.md).

To check a solution with the independent checker:

```console
make tools
tools/target/release/checker instance.in solution.out
# VALID benefit=18471 revenue=30862 travel_cost=12391 passengers=76 parcels=78
```

## How it works

1. A greedy insertion pass builds a first plan within 0.5 seconds.
2. For at most 10 requests and 12 taxis, exhaustive search per taxi and a subset dynamic program give a proven optimum.
3. For at most 512 requests and 63 taxis, route enumeration feeds a set-packing master solved with integer-scaled prices and clique cuts. A proof needs a complete route pool, otherwise the routes still seed the search.
4. ALNS with seven destroy and eight repair operators runs until a quarter of the time limit.
5. The SISR phase takes the remaining three quarters. Every 10 seconds it recombines archived routes, and every 2.5 seconds it reassigns whole routes to taxis with the Hungarian algorithm.

[docs/algorithm.md](docs/algorithm.md) explains each phase and its parameters.

## Code map

| Module | Responsibility |
|---|---|
| [`types.hpp`](src/types.hpp), [`state.hpp`](src/state.hpp) | Events, routes, plans, and the global problem state |
| [`solution.hpp`](src/solution.hpp) | Route evaluation, plan validation, incumbent and route archive |
| [`insertion.hpp`](src/insertion.hpp) | Insertion moves, candidate taxis, request removal |
| [`alns_operators.hpp`](src/alns_operators.hpp), [`local_search.hpp`](src/local_search.hpp) | ALNS destroy and repair, local search |
| [`recombination.hpp`](src/recombination.hpp) | Set packing over archived routes |
| [`exact_small.hpp`](src/exact_small.hpp), [`exact_sparse.hpp`](src/exact_sparse.hpp) | Exact methods |
| [`sisr.hpp`](src/sisr.hpp) | String removals, route reassignment |
| [`solver.hpp`](src/solver.hpp), [`input.hpp`](src/input.hpp), [`main.cpp`](src/main.cpp) | Pipeline, input, command line |

## Repository layout

```text
contest/      the file exactly as submitted, with its SHA-256
src/          the same program split by component and formatted
tools/        Rust: checker, instance generator, amalgamation, equivalence check, benchmark runner
tools/tests/  Rust integration tests, including a brute-force optimum oracle
benchmarks/   instance definitions and recorded results
docs/         algorithm, architecture, problem and formal model, benchmarks
```

## Tests and CI

```console
make test          # solver against brute force, validity, command line, tools
make equivalence   # src/ is the submitted program
make sanitize      # AddressSanitizer and UBSan runs
make ci            # format, lint, build, test, equivalence, contest file checks
```

The exact method is compared with an exhaustive search written separately from the solver. The checker that validates every output shares no code with the solver either. The GitHub workflows call these same make targets, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Reproducing the benchmarks

The generator is deterministic and reproduces the instances used during development byte for byte.

```console
bash benchmarks/generate.sh build/benchmarks
tools/target/release/bench --solver build/solver --instances build/benchmarks \
  --time 295 --seeds 42 --jobs 1 --out results.csv
```

Details and the measured results are in [docs/benchmarks.md](docs/benchmarks.md).

## License

MIT, see [LICENSE](LICENSE). The algorithms follow the references listed in [docs/algorithm.md](docs/algorithm.md).
