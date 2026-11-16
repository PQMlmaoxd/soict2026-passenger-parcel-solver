# Architecture

## One program, two forms

The contest accepts one C++ file. The repository therefore holds the same program twice:

| Form | Where | Purpose |
|---|---|---|
| Submitted file | `contest/SOICT2026_Round2_S4T2A8_SISR_v3_pkasg.cpp` | Byte for byte what was uploaded. Never edited. SHA-256 is in `contest/SHA256SUMS`. |
| Modular source | `src/` | The same text split by component, formatted with clang-format, with comments added. This is what people read and build. |

`make equivalence` shows that the two are one program. It preprocesses both files, removes all whitespace and compares the token streams. Comments are gone after preprocessing, so only a change of code can make the streams differ. With `ASM=1` it also compiles both with `-O2 -S` and compares the assembly. Both checks pass for `src/main.cpp` and for the single file written by `make amalgamate`, with g++ 13 and 14.

The checks have limits. Equal tokens mean equal semantics for any conforming compiler. Equal assembly is evidence about one compiler version, and it compares two builds made by the same compiler, so it says nothing across compilers. Neither check says the program is correct, the tests do that.

## Module order

`main.cpp` includes the modules in a fixed order, and a module may only use what the modules above it declare. The order is the order of the original file, which is why the equivalence check can work on the token stream at all.

```text
common         aliases, macros, constants
fast_input     integer reader
types          Req, Node, Rt, Sol, Ins, Col, ALNS
state          globals, clock, random numbers, matrix accessors
solution       eval, check, route archive, incumbent update
insertion      findMove, taxiList, eraseReq
alns_operators repair and ruin
local_search   optRoute, crossTail, optBlock, ls
recombination  pack, globalPack
exact_small    exactDfs
exact_sparse   route enumeration and the set-packing master
input          instance reader
sisr           string removals, route reassignment
solver         pipeline, output, recovery
main           command line
```

## Design decisions

### Global state

The instance and the incumbent are globals. Every routine reads the same matrices, and the one-file contest build has no use for indirection. The cost is that modules are not independently testable, which is why the tests drive the binary.

### Short identifiers

Names such as `K`, `R`, `Q`, `O`, `tmx` and `ev` follow the mathematical notation of the problem: taxis, requests, capacity, depot. They are listed at the top of `state.hpp` and `types.hpp`. Renaming them would change the token stream and break the equivalence proof, so this refactor is limited to layout, comments and file structure.

### Wall-clock phases

Phase budgets are fractions of elapsed wall time. A run is therefore not bit-reproducible, even with the same seed. For debugging, setting `SISR_WARMIT` swaps the clock for a tick counter. The contest measures CPU time of a single thread, which is why the solver never spawns threads.

### Defensive output

The solver keeps a valid plan at all times, validates it before printing, and exits with `_Exit` to skip the teardown of large containers. A bad input or argument returns 1 before anything is written.

## Tools

The `tools/` crate is plain Rust with no dependencies.

| Binary | Role |
|---|---|
| `checker` | Independent simulator of the contest rules, shares no code with the solver |
| `gen_instance` | Instance generator, deterministic, compatible with the Python generator used in the first benchmarks |
| `amalgamate` | Writes the single contest file from `src/` |
| `check_equivalence` | Token and assembly comparison described above |
| `bench` | Paired runs of several solver builds, every output checked, CSV and summary |

`tools/src/mt.rs` reimplements the Mersenne Twister and the helpers of Python's `random` module. This keeps instances produced before the Rust tools existed reproducible, and `tools/tests/golden/` pins three of them.

## Tests

The tests live in `tools/tests/` and run against the real binary.

* `exact_method_matches_brute_force` compares the solver with an exhaustive search over all plans on ten tiny instances. The oracle reuses the checker for route simulation and shares nothing else with the solver.
* The heuristic test runs `--no-exact` on tiny instances and expects the optimum as well.
* Validity, wall-clock and command-line tests cover bad arguments, malformed input, zero time and an instance without requests.
* The equivalence tests check that a changed constant is detected and that added comments are not.

## Changing the program

Any edit under `src/` that is more than layout makes `make equivalence` fail on purpose. To publish a new contest build:

1. Make the change in `src/` and run `make test sanitize`.
2. Run `make amalgamate` and copy `build/solver.cpp` to a new file under `contest/`, with its own name.
3. Update `contest/SHA256SUMS`, `CONTEST_FILE` in the Makefile and the default in `tools/src/bin/check_equivalence.rs`.
4. Add an entry to `CHANGELOG.md` with the measured effect and its scope.
