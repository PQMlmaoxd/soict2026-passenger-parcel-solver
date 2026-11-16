# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Every entry that touches the solver states its measured effect and the scope of that measurement.

## [1.0.0] - 2026-11-16

First public version. It contains the program submitted to SOICT 2026 Round 2.

### Solver

- Submitted file `contest/SOICT2026_Round2_S4T2A8_SISR_v3_pkasg.cpp`, SHA-256 `0ca1e99c...3429b`. Official CMS score 512,260 against 503,071 for the Fleet baseline.
- Differences from Fleet: string-removal phase from a quarter of the time limit, route archive with periodic recombination adopted only on strict improvement, Hungarian reassignment of routes to taxis every 2.5 seconds, overflow guard in the reassignment cost matrix.

### Repository

- `src/`: the submitted program split by component and formatted. Proven identical to the submitted file by token stream and by assembly.
- `tools/`: Rust checker, instance generator, amalgamation, equivalence check and benchmark runner, with integration tests that compare the solver with a brute-force optimum.
- `benchmarks/`: 87 generated configurations and the recorded comparison with Fleet.
- CI: format and lint, compiler matrix, sanitizers, equivalence, contest file checks.
