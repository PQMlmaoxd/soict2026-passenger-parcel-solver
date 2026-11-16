# Benchmarks

| File | Content |
|---|---|
| `mimic-configs.tsv` | The 87 generator settings of the mimic suite: name, K, N, M, seed, layout, horizon, window, asymmetry |
| `results/mimic-2026-11-15.csv` | Fleet, Pool and this solver on each configuration, one 295 s run each, every output checked |
| `generate.sh` | Regenerates the instances, optionally only those whose name matches a pattern |

```console
bash benchmarks/generate.sh build/benchmarks 's1_0[1-4]'     # four instances
```

How the suite was built, what the numbers mean and what they do not show is in [../docs/benchmarks.md](../docs/benchmarks.md).
