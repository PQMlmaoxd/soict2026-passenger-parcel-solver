# Solver source

The submitted program, split by component. `main.cpp` includes the modules in a fixed order and a module may only use what the ones above it declare. The order and a one-line description of each module are in [../docs/architecture.md](../docs/architecture.md).

Rules for this directory:

- Layout and comments may change freely. Code may not: `make equivalence` compares the preprocessed token stream with [`../contest`](../contest).
- `make format` applies the clang-format style from `../.clang-format`.
- Every route that is built or edited must pass `eval()`, and the incumbent `ans` only changes through `upd()`.
