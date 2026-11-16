# Algorithm

The solver runs a fixed pipeline against one deadline. Each phase has a time budget, hands its best plan to the next one, and can stop early when it proves optimality. [problem.md](problem.md) defines the problem.

Notation: `K` taxis, `R` requests, `D = TL` the internal deadline. `TL` is `--time` minus `min(0.15 s, 2%)`, so `D = 294.85 s` with the default 295 s.

## Pipeline

| Step | Condition | Budget | Module |
|---|---|---|---|
| Greedy start | always | `min(0.5 s, 2.5% of D)` | `solver.hpp` |
| Exhaustive search and subset DP | `R <= 10`, `K <= 12` | `min(12 s, 25% of D)` | `exact_small.hpp` |
| Route enumeration and set packing | `R <= 512`, `K <= 63`, revenues pass the range guard | 12 s, or up to 120 s when `R > 256` or `K > 24` | `exact_sparse.hpp` |
| Initial repair | always | `min(5 s, 8% of D)` | `alns_operators.hpp` |
| ALNS | until `0.25 D` | the time left | `alns_operators.hpp`, `local_search.hpp` |
| SISR | from `0.25 D` to `D` | the time left | `sisr.hpp` |

If an exact method finishes in time, its plan is optimal and the solver stops. If it stops at a cap, the routes it found still feed the incumbent and the route archive. If exact search itself ends after `0.25 D`, ALNS gets no iterations and SISR starts at once.

## Model

A passenger is one event: the pickup, the direct ride and the drop-off happen back to back, so it occupies a single slot in a route. A parcel is two events, pickup and delivery. Every route evaluation goes through `eval()`, which simulates the route, checks windows and load, and returns the benefit together with the slack data that the insertion code reuses:

* `tm[i]`, the time at which event `i` completes,
* `w[i]`, the parcel load after event `i`,
* `lim[i]`, the latest time event `i` may start without breaking any later event.

With `lim`, inserting a request at a position tests the unchanged suffix of a route in constant time. Everything that edits a route calls `eval()` before the route can enter a plan, because on nonmetric input no shortcut argument can be trusted.

## Exact methods

### Small instances

For each taxi, a depth-first search enumerates event orders and keeps, for each set of served requests, the best route. A prefix is dropped when another prefix with the same picked and delivered sets and the same last event finishes no later and earns at least as much. A subset dynamic program then assigns one request set to each taxi without sharing a request. Taxis with the same depot and capacity share one search.

### Larger instances

The enumeration works per taxi under caps of 250,000 columns overall and 30,000 request sets per taxi. It prunes with shortest-path travel times, which are valid lower bounds even when the matrices break the triangle inequality, and with the remaining revenue as an upper bound on any completion. The master problem picks at most one route per taxi and each request at most once:

```
maximize   sum of p[k][r] * x[k][r]
subject to sum over r of x[k][r] <= 1              for each taxi k
           sum over (k, r) serving i of x[k][r] <= 1   for each request i
```

Three components work on it. `reducedMaster` is a branch and bound that uses request prices scaled by 1024 so that all bounds are exact integers. `refineCliquePrices` improves the prices with projected subgradient steps. `stableClique` adds clique rows over conflicting columns and solves the relaxation with a small simplex. A proof of optimality needs a complete column pool.

## ALNS

The ALNS phase follows Ropke and Pisinger. It keeps a current plan next to the best one.

| Operators | Choices |
|---|---|
| Destroy (7) | random, related, best removal gain, route removal, parcel-biased related, route segment, pickup deadline |
| Repair (8) | greedy, regret-2, regret-3, noisy greedy, affinity, exploratory, delay ratio, delay penalty |

Operators are drawn in proportion to weights. Every 40 iterations a weight moves to `max(0.1, 0.8 w + 0.2 r)`, with `r` the mean reward: 12 for a new best plan, 5 for improving the current one, 1 for another accepted trial, 0.1 for a rejection. Every third iteration a local search runs: event relocation and segment reversal inside a route, exact reordering of a window of up to 11 events, tail exchange between two routes at cuts with no parcel on board, and reinsertion of single requests or pairs. A trial is accepted if it is not worse, or with probability `exp(delta / T)` under a cyclic temperature. After `max(1 s, 0.12 D)` without a new best plan, the search restarts from the best plan with probability 0.7, otherwise from an empty plan.

Taxis considered for a request are all of them when `K <= 24`. Otherwise the 16 closest by pickup proximity and a learned request-to-taxi affinity, the 6 that arrive earliest, and 4 random ones.

## Route archive and recombination

Accepted routes go to a per-taxi archive that keeps the best route per request set, with a capacity of `max(16, min(160, 12000 / K))`. When the archive is full it evicts the weakest route, three times out of four among the most crowded size class so the archive stays diverse.

Recombination picks routes from the archive that share no request:

* `pack` searches combinations over at most 12 taxis, the others fixed.
* `globalPack` runs simulated annealing over one archived route per taxi for the whole fleet. It applies when `120 < R <= 512` and `K >= 8`.

Both return the incumbent unless they find a strictly better plan.

## SISR

The SISR phase uses the ruin-and-recreate scheme of Christiaens and Vanden Berghe. A ruin step draws a seed request and removes one string of consecutive requests from each of up to `k_s` routes near it, with `k_s` and the string length derived from the average route length (`c = 10`, `Lmax = 10`). With probability 0.5 a string keeps a block of consecutive requests inside its window. Whole requests are removed, so a parcel never loses only one of its stops. Recreate inserts the removed requests and up to five unserved neighbors in random, revenue or proximity order, each at its best positive-gain position.

Three additions run inside the loop:

* With probability 0.25 a step is an exchange: an unserved request replaces one of its 12 nearest served requests on the same taxi if that pays.
* Every 10 seconds, archive recombination runs for one second. The plan is only replaced on a strict improvement.
* Every 2.5 seconds, `sisrAssignRoutes` reassigns the routes of the best plan to taxis within 35 ms. Event orders stay fixed, so only the leg from the depot changes. Route `i` may go to taxi `k` when its peak load fits `Q[k]` and the depot leg meets the first event's latest start. The Hungarian algorithm minimizes total depot-leg cost, every reassigned route is evaluated again, and the result is adopted only if the benefit grows.

Trials are accepted by simulated annealing with the temperature falling geometrically from `0.3 g` to `0.01 g`, where `g` is the median request revenue.

## Parameters

The defaults are the values of the contest submission, and the contest runs without any of these variables set. They exist so that experiments do not need a rebuild.

| Variable | Default | Meaning |
|---|---|---|
| `SISR_START` | 0.25 | Fraction of the deadline at which SISR starts |
| `SISR_CBAR`, `SISR_LMAX` | 10, 10 | Average removed requests per ruin, maximum string length |
| `SISR_BETA` | 0.01 | Split-string parameter |
| `SISR_UNS` | 5 | Unserved neighbors added to the recreate list |
| `SISR_SWAP` | 0.25 | Probability of an exchange step |
| `SISR_BLINK` | 0.01 | Probability of skipping an improving insertion position |
| `SISR_T0`, `SISR_TF` | 0.3, 0.01 | Start and end temperature, in units of `g` |
| `SISR_PACK`, `SISR_PACKT` | 10, 1.0 | Seconds between recombinations, seconds each may take |
| `SISR_PSAVE` | 8 | Archive changed routes after every N accepted trials |
| `SISR_PATIENCE` | off | Stop SISR after this many seconds without improvement |
| `SISR_PKMODE` | 1 | Whether recombination may reset the current plan |
| `SISR_MINNODES`, `SISR_MINLEN` | 0, 0 | Gate on the plan size at the switch, open by default |
| `SISR_MAXIT`, `SISR_WARMIT` | 0, 0 | Iteration limits for deterministic debugging runs |

## Guarantees and failure handling

* The incumbent only receives plans whose routes passed `eval()`. `print()` validates it again with `check()` before writing anything.
* If a phase throws, `recoverAns()` falls back to the snapshot taken before SISR, and to an empty plan as a last resort. The process never prints a half-built plan.
* Memory stays near 68 MB at `K = N = M = 250`, the largest instance size of the contest.

## References

* S. Ropke, D. Pisinger. An adaptive large neighborhood search heuristic for the pickup and delivery problem with time windows. *Transportation Science* 40(4), 2006.
* J. Christiaens, G. Vanden Berghe. Slack induction by string removals for vehicle routing problems. *Transportation Science* 54(2), 2020.
* C. Friedrich, R. Elbert. Adaptive large neighborhood search for vehicle routing problems with transshipment facilities arising in city logistics. *Computers & Operations Research* 137, 2022.
* H. W. Kuhn. The Hungarian method for the assignment problem. *Naval Research Logistics Quarterly* 2(1-2), 1955.
