# Benchmarks

Every number carries a scope label. Gains measured on generated instances have repeatedly shrunk on the contest, and some candidates that won on the instances used to choose them lost on fresh ones. Read the labels before comparing numbers.

| Label | Meaning |
|---|---|
| official CMS | Score reported by the contest platform |
| synthetic development | Generated instances used while developing, 295 s per run, paired against the baseline |
| held-out synthetic | Generated instances that were not used to choose the candidate |

## Official CMS

| Submission | Score | Note |
|---|---|---|
| Fleet (`3dce29`) | 503,071 | Baseline, 39 tests valid |
| This solver, `contest/` file | 512,260 | Final rank 6. Rank 3 on the scoreboard of 15/11/2026, 21:32 ICT |

The difference is +9,189 points. Eight tests run to the deadline for Fleet and add up to 265,735, so the gain is 3.46% of those eight if the other 31 tests are unchanged. At the 21:32 snapshot the margin to rank 4 was 130 points. Other teams improved after that, which moved this submission down to 6th. The score itself did not change, because no further submission was made.

## Mimic suite (synthetic development)

The contest tests are hidden, so a mimic suite stands in for them. Each CMS test left a fingerprint: the score of Fleet, the score of an earlier team submission called Pool (420,961 on the contest, built around route pools), and the memory they used. The ratio of Pool to Fleet varied a lot between the long tests, from 0.57 to 0.84. Generator settings were searched until the same ratio appeared, on the assumption that instances that hurt Pool as much are of a similar kind.

The suite has 87 configurations, listed in [`benchmarks/mimic-configs.tsv`](../benchmarks/mimic-configs.tsv). Stage 1 varies the number of taxis (30, 60, 100), the layout (uniform, clustered, hub) and the time-window settings at about 435 requests. Stage 2 varies the instance size (V = 220, 320 and 580 vertices, about 100, 160 and 290 requests) with 5, 15 and 40 taxis. Each configuration ran once per solver, 295 seconds, 12 runs in parallel on one Windows machine, g++ 14.2 with `-std=c++17 -O2 -static`. All 261 outputs passed the independent checker. The per-run table is [`benchmarks/results/mimic-2026-11-15.csv`](../benchmarks/results/mimic-2026-11-15.csv).

Difference of this solver to Fleet, in percent of the Fleet score:

| Subset | Configurations | Mean | Minimum | Losses |
|---|---|---|---|---|
| All | 87 | +2.75 | +0.11 | 0 |
| Stage 1 | 36 | +3.03 | +0.98 | 0 |
| Stage 2 | 51 | +2.55 | +0.11 | 0 |
| 30 taxis | 12 | +4.36 | | 0 |
| 60 taxis | 12 | +2.67 | | 0 |
| 100 taxis | 12 | +2.06 | | 0 |
| Pool at less than 0.8 of Fleet | 18 | +4.19 | | 0 |

By size in stage 2: 220 vertices +1.15% (15 configurations), 320 vertices +2.10% (18), 580 vertices +4.17% (18, minimum +1.34%). The gain shrinks as the number of taxis grows and grows with the size of the instance and with how badly Pool does.

These are single runs and run-to-run noise is about 0.3 to 0.5% per instance, so individual rows say little and the averages say more. The suite is a proxy. Fleet itself gained 3.96% over an earlier submission on generated instances and 1.50% on the contest, so a gain here does not carry over in full.

## Distance to known optima (synthetic development)

On generated instances small enough to enumerate completely, the optimum is known. At 295 seconds this solver ended on average 0.99% below it, while Fleet ended 2.6 to 5.8% below. This comes from the development notes and is not reproduced by the files in this repository.

## Candidates that did not help

Each was measured paired against the baseline at 295 seconds. A change was kept only if it also held on instances it was not chosen on.

| Change | Result | Scope |
|---|---|---|
| Assign routes on the running plan instead of the incumbent | +0.26% on the selection set, then 0.12% worse on held-out instances | synthetic development, held-out |
| SISR from 15% of the time instead of 25% | +0.02% on fresh instances, −0.27% in an earlier test | synthetic development |
| Two independent SISR chains sharing the archive | −0.31%, worse on 4 of 4 instances | synthetic development |
| Average removed requests 6 or 15 instead of 10 | −0.05% and −0.17% | synthetic development |
| Maximum string length 20 instead of 10 | −0.08% | synthetic development |
| Stop SISR after 40 s without improvement | −1.96%, worst −5.47% | synthetic development |
| Four times the time (1180 s instead of 295 s) | +0.44% on 6 instances | synthetic development |

The last row marks the ceiling of local tuning: four times the compute buys less than half a percent, so the gain over Fleet owes more to the search design than to compute.

## Time and memory

On a stress instance with `K = N = M = 250` the solver stays valid, finishes in 294.9 s wall time and uses about 68 MB. Peak memory on the largest contest tests of the baseline was 60.7 MB.

## Reproducing

```console
make build tools
bash benchmarks/generate.sh build/benchmarks      # 87 instances, byte-identical to the originals
tools/target/release/bench \
  --solver build/solver \
  --instances build/benchmarks \
  --time 295 --seeds 42 --jobs 1 --out results.csv
```

Add a second `--solver` to compare two builds: the first is the baseline and the summary reports mean difference, wins, losses and the worst pair. Use at most one job per physical core, because the contest counts CPU time of a single thread and parallel runs on shared cores distort it. A full pass of 87 instances at 295 seconds takes about 7 hours with one job.
