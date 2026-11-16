# Problem, formal model and file formats

This page states the rules as the independent checker (`tools/src/checker.rs`) enforces them, then gives the problem as a formal model and lists the file formats. The checker is the reference when a document and the code disagree.

## The problem

There are `K` taxis, `N` passengers and `M` parcels. `R = N + M` requests share `V = K + 2R` vertices: each request has its own pickup vertex and its own drop-off vertex, and no two requests share a vertex. Each taxi starts at its own depot vertex and has a parcel capacity.

A plan gives every taxi a route, a sequence of service vertices. A request is served completely by one taxi or not at all. An idle taxi has an empty route, and an empty plan is valid.

| Rule | Passenger | Parcel |
|---|---|---|
| Pickup and drop-off | Drop-off immediately follows the pickup on the same taxi | Pickup comes before the delivery on the same taxi, other stops may lie between |
| Pickup window | Start within `[earliest, latest]`; arriving early means waiting | Start within `[pickupEarliest, pickupLatest]` |
| Drop-off window | None | Start within `[dropEarliest, dropLatest]` |
| Load | Does not use capacity | Adds its weight at pickup, removes it at delivery. The load never exceeds the taxi capacity and ends at zero |
| Service time | Same value at pickup and at drop-off | Same value at pickup and at delivery |

Time is simulated per taxi. A taxi leaves the previous vertex at its departure time, arrives after the travel time of that arc, waits until the window opens if it is early, and leaves after the service time. The latest times are limits on the start of service, so arriving after them is infeasible.

The benefit of a plan is the sum of the revenues of the served requests minus the cost of every arc driven. The first arc, from the depot to the first stop, counts. There is no return to the depot. The travel-time and travel-cost matrices are directed and need not satisfy the triangle inequality, so shortcuts and stop removal have to be re-checked.

## Formal model

Data. Taxi `k` starts at depot vertex `o_k` with capacity `Q_k`. Request `i` has revenue `rho_i`, a service time `s_i` and the windows from the table above. A passenger is one block of two vertices, a parcel is a pickup vertex and a delivery vertex with weight `w_i`. The directed matrices `t(u, v)` and `c(u, v)` give travel time and cost.

A route for taxi `k` is a vertex sequence `v_1 ... v_m`. Put `v_0 = o_k`, a departure time `d_0 = 0` and a load `l_0 = 0`. For `j = 1 ... m`:

```text
a_j = d_{j-1} + t(v_{j-1}, v_j)          arrival
b_j = max(a_j, open_j)                   service start, waits when early
require b_j <= close_j                   the latest times bound the start of service
d_j = b_j + s_j                          departure
l_j = l_{j-1} + w   at a parcel pickup,  l_{j-1} - w   at a parcel delivery
require l_j <= Q_k
```

A passenger drop-off has no window (`open = -inf`, `close = +inf`), and its vertex must directly follow the pickup. A route also requires that every parcel pickup precedes its delivery and that the load ends at zero. The benefit of a feasible route is the revenue of its requests minus `sum_j c(v_{j-1}, v_j)`, which includes the arc from the depot and no return arc. Call the set of feasible routes of taxi `k` `Omega_k`. It always contains the empty route with benefit 0.

The problem is to choose one route per taxi so that no request is served twice, maximizing the total benefit. As a route-based integer program, with `x[k, w] = 1` when taxi `k` drives route `w`:

```text
maximize    sum over k, w in Omega_k of  benefit(k, w) * x[k, w]
subject to  sum over w of x[k, w] <= 1                       for each taxi k
            sum over k, w serving request i of x[k, w] <= 1  for each request i
            x binary
```

`Omega_k` grows exponentially with the number of requests, so only small instances can list it. [algorithm.md](algorithm.md) describes how the exact methods enumerate a subset of it and how the heuristics search the space of plans without listing it.

## Properties that shape the solver

* Requests are optional. A request is worth serving only if its revenue exceeds the extra travel it causes, so choosing who is served is part of the problem, next to ordering the stops.
* The problem is NP-hard. With one taxi, no parcels, no windows and revenues larger than any route cost, serving everyone is optimal and what remains is a shortest Hamiltonian path on an asymmetric cost matrix.
* Feasibility is not monotone. Removing a stop can make a route infeasible, because the matrices need not satisfy the triangle inequality and waiting times change with the new arrival times. Every move is therefore checked by simulating the route.
* A passenger takes no capacity but blocks the taxi in time, since no parcel stop may fall between its pickup and drop-off. Passengers and parcels compete for time, parcels also compete for capacity.

## Related problems

The model is a profit-maximizing variant of the pickup and delivery problem with time windows (PDPTW) in which only some requests need to be served. The passengers resemble dial-a-ride requests, except that a passenger rides alone and directly. The parcels are the classical capacitated pickup and delivery requests. Background reading:

* M. W. P. Savelsbergh, M. Sol. The general pickup and delivery problem. *Transportation Science* 29(1), 1995.
* J.-F. Cordeau, G. Laporte. The dial-a-ride problem: models and algorithms. *Annals of Operations Research* 153, 2007.

## Input

Whitespace-separated integers. A `#` starts a comment up to the end of the line.

```text
K N M
depot_1 capacity_1                      # K lines
...
pickup drop earliest latest service revenue                                          # N passenger lines
pickup drop weight earliest latest dropEarliest dropLatest service revenue           # M parcel lines
time[1][1] ... time[1][V]               # V lines, travel time
...
cost[1][1] ... cost[1][V]               # V lines, travel cost
```

Vertices are 1-based. All values are non-negative integers that fit in a signed 64-bit integer, windows satisfy `earliest <= latest`, and the solver rejects `V > 20000` to bound the memory of the two dense matrices.

## Output

```text
benefit
count_1 v v v ...                       # one line per taxi, in input order
...
count_K v v v ...
```

`count` is the number of vertices on the line. A passenger appears as its pickup vertex followed by its drop-off vertex, a parcel as two separate vertices. An idle taxi prints `0`.

Illustration for two taxis. The first serves a parcel (vertices 3 and 4) and, between its pickup and delivery, a passenger (vertices 7 and 8):

```text
162
4 3 7 8 4
0
```

## Checking a solution

```console
tools/target/release/checker instance.in solution.out
```

The checker prints `VALID` with the recomputed benefit, revenue, travel cost and the number of served requests, or `INVALID` with the first broken rule. The exit code is 0, 1 or 2 for valid, invalid and unreadable input. The declared benefit on the first line has to equal the recomputed one.
