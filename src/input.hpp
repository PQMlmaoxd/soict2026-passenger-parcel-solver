#pragma once

// Reads the instance from stdin and derives the event table.
//
// Layout, all integers: "K N M", then K lines "depot capacity", then N
// passenger lines "pickup dropoff earliest latest service revenue", then M
// parcel lines "pickup dropoff weight earliest latest earliestDrop latestDrop
// service revenue", then the V x V time matrix and the V x V cost matrix.
// Vertices are 1-based in the file and 0-based inside the solver.

void input() {
  FastIn in;
  auto dimension = [&]() {
    ll x = in.next();
    if (x < 0 || x > 1000000)
      throw runtime_error("Invalid dimension");
    return int(x);
  };
  K = dimension();
  N = dimension();
  M = dimension();
  R = N + M;
  V = 2 * R + K;
  if (V > 20000)
    throw runtime_error("Dense matrices exceed supported memory (V > 20000)");
  O.resize(K);
  Q.resize(K);
  rq.resize(R);
  ev.resize(2 * R);
  pool.resize(K);
  auto vertex = [&]() {
    ll x = in.next();
    if (x < 1 || x > V)
      throw runtime_error("Vertex outside 1..V");
    return int(x - 1);
  };
  auto nonnegative = [&]() {
    ll x = in.next();
    if (x < 0)
      throw runtime_error("Expected nonnegative data");
    return x;
  };
  for (int k = 0; k < K; k++) {
    O[k] = vertex();
    Q[k] = nonnegative();
  }
  vl rev;
  for (int r = 0; r < R; r++) {
    auto& q = rq[r];
    q.p = vertex();
    q.d = vertex();
    if (!isP(r))
      q.w = nonnegative();
    q.e = nonnegative();
    q.l = nonnegative();
    if (!isP(r)) {
      q.ed = nonnegative();
      q.ld = nonnegative();
    }
    q.s = nonnegative();
    q.rev = nonnegative();
    rev.push_back(q.rev);
  }
  tmx.resize(size_t(V) * V);
  cst.resize(size_t(V) * V);
  for (auto& x : tmx)
    x = nonnegative();
  for (auto& x : cst)
    x = nonnegative();
  for (int r = 0; r < R; r++) {
    const auto& q = rq[r];
    if (isP(r))
      ev[2 * r] = {q.p, q.d, q.e, q.l, 2 * q.s + t(q.p, q.d), 0, q.rev, c(q.p, q.d)};
    else {
      ev[2 * r] = {q.p, q.p, q.e, q.l, q.s, q.w, 0, 0};
      ev[2 * r + 1] = {q.d, q.d, q.ed, q.ld, q.s, -q.w, q.rev, 0};
    }
  }
  if (R) {
    nth_element(rev.begin(), rev.begin() + R / 2, rev.end());
    gs = max(1.0, double(rev[R / 2]));
    vl durations;
    for (const auto& q : rq)
      durations.push_back(t(q.p, q.d) + 2 * q.s);
    nth_element(durations.begin(), durations.begin() + R / 2, durations.end());
    gt = max(1.0, double(durations[R / 2]));
  }
  ph.assign(size_t(R) * K, 0.5);
  ans = emptySol();
}
