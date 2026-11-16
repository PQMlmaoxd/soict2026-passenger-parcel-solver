#pragma once

// Problem data and run state. These are globals on purpose: every routine
// reads the same instance and the single-file contest build gets no
// indirection cost.
//
// Notation:
//   K, N, M   taxis, passengers, parcels;  R = N + M requests, V = 2R + K vertices
//   O[k], Q[k]  depot vertex and parcel capacity of taxi k
//   tmx, cst  dense V x V travel-time and travel-cost matrices, read through
//             t(u, v) and c(u, v). They may be asymmetric and nonmetric.
//   rq, ev    requests and events (see types.hpp)
//   pool[k]   archive of positive-benefit routes seen for taxi k
//   ph        request-to-taxi affinity learned from elite plans, read via tau(r, k)
//   ans       the incumbent: best feasible plan found so far
//   TL        internal deadline in seconds (the --time value minus a margin)
//   gs, gt    scale constants: median request revenue and median direct duration
//
// detClock replaces the wall clock by a tick counter. It is only switched on by
// the SISR_WARMIT environment variable, for deterministic debugging runs.

int K = 0, N = 0, M = 0, R = 0, V = 0;
vi O;
vl Q, tmx, cst;
vc<Req> rq;
vc<Node> ev;
vc<vc<Col>> pool;
vc<double> ph;
Sol ans;
mt19937_64 rng;
clk::time_point st;
double TL = 294.9, gs = 1, gt = 1;
uint64_t iter = 0;
bool ex = true;

static bool detClock = false;
static uint64_t detTicks = 0;

double getTime() {
  if (detClock)
    return double(++detTicks) * 1e-9;
  return chrono::duration<double>(clk::now() - st).count();
}

bool out(double until) {
  return getTime() >= min(until, TL);
}

double rnd() {
  return generate_canonical<double, 53>(rng);
}

int rd(int n) {
  return n ? int(rng() % uint64_t(n)) : 0;
}

ll t(int u, int v) {
  return tmx[size_t(u) * V + v];
}

ll c(int u, int v) {
  return cst[size_t(u) * V + v];
}

double& tau(int r, int k) {
  return ph[size_t(r) * K + k];
}

bool isP(int r) {
  return r < N;
}
