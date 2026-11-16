#pragma once

// Top-level pipeline: greedy start, exact methods, ALNS, then SISR.
//
// solve() follows the pipeline table in docs/algorithm.md. `ans` only ever receives plans whose
// routes passed eval(). print() re-validates it with check() before writing, and
// if a phase throws, recoverAns() falls back to the last valid snapshot.

static Sol gSafe; // best valid solution snapshot taken before the SISR phase (fallback if ans is
                  // left corrupt by an exception)

// Runs the full pipeline until the deadline.
void solve() {
  if (!R || !K || out(TL))
    return;
  if (all_of(all(rq), [](const Req& q) { return q.rev == 0; }))
    return;
  vi order(R);
  iota(all(order), 0);
  sort(all(order), [&](int a, int b) { return rq[a].rev > rq[b].rev; });
  Sol seed = ans;
  double bootstrapEnd = min(TL, getTime() + min(0.5, 0.025 * TL));
  for (int r : order) {
    if (out(bootstrapEnd))
      break;
    auto p = bestMove(seed, r, bootstrapEnd);
    if (p.ok() && p.gain > 0) {
      put(seed, r, p);
      upd(seed);
    }
  }
  if (ex && R <= 10 && K <= 12) {
    double budget = min(12.0, max(0.01, 0.25 * TL));
    if (exactDfs(min(TL, getTime() + budget)))
      return;
  } else if (ex && R <= 512 && K <= 63) {
    double budget =
        R > 256 || K > 24 ? min(120.0, max(0.01, 0.8 * TL)) : min(12.0, max(0.01, 0.15 * TL));
    if (exactSparse(min(TL, getTime() + budget)))
      return;
  }
  Sol current = ans;
  double initialEnd = min(TL, getTime() + min(5.0, max(0.01, 0.08 * TL)));
  repair(current, 1, {}, initialEnd, true);
  upd(current);
  for (int k = 0; k < K; k++)
    save(k, current.a[k]);
  ALNS destruction(7), reconstruction(8);
  double nextPack = getTime() + min(4.0, max(0.04, 0.04 * TL));
  double lastBest = getTime();
  ll previousBest = ans.f;
  const double Tsw = min(TL, envd("SISR_START", 0.25) * TL);
  const double minNodes = envd("SISR_MINNODES", 0);
  auto stops = [&] {
    ll n = 0;
    for (const auto& z : ans.a)
      for (int x : z.a)
        n += isP(x / 2) ? 2 : 1;
    return n;
  };
  const double minLen = envd("SISR_MINLEN", 0);
  auto longRoutes = [&] {
    ll n = 0;
    for (const auto& z : ans.a)
      n += !z.a.empty();
    return double(stops()) >= minLen * double(max(1LL, n));
  };
  const uint64_t warmit = uint64_t(envd("SISR_WARMIT", 0));
  bool decided = false;
  while (!out(TL)) {
    if (!decided && (warmit ? iter >= warmit : getTime() >= Tsw)) {
      decided = true; // the gate is evaluated exactly once
      const bool go = double(stops()) >= minNodes && longRoutes();
      cerr << "event=gate open=" << go << " stops=" << stops() << " t=" << getTime() << "\n";
      if (go) {
        gSafe = ans;
        try {
          sisrPhase(Tsw);
        } catch (...) { // SISR caches are locals of sisrPhase: already released by unwinding
          bool good = false;
          try {
            good = check(ans);
          } catch (...) {
          }
          if (!good)
            swap(ans, gSafe); // no allocation
          cerr << "event=sisr_exception good=" << good << "\n";
        }
        if (out(TL))
          break;
        current = ans;
        for (int k = 0; k < K; k++)
          save(k, current.a[k]);
        lastBest = getTime();
        previousBest = ans.f;
        continue;
      }
    }
    iter++;
    double remaining = TL - getTime();
    double slice = min(remaining, max(0.015, min(1.0, 0.006 * TL)));
    double until = min(TL, getTime() + slice);
    bool polish = iter % 3 == 0;
    double repairEnd = polish ? getTime() + max(0.0, until - getTime()) * 0.65 : until;
    int d = destruction.pick(rng), r = reconstruction.pick(rng);
    Sol trial = current;
    int active = int(used(trial).size());
    double stagnation = getTime() - lastBest;
    double fraction = stagnation > max(1.0, 0.05 * TL) ? 0.15 + 0.55 * rnd() : 0.05 + 0.30 * rnd();
    int amount = max(1, min(120, int(ceil(active * fraction))));
    auto removed = ruin(trial, d, amount, repairEnd);
    repair(trial, r, removed, repairEnd, false, &current.id);
    if (polish && !out(until))
      ls(trial, until);
    upd(trial);
    ll delta = trial.f - current.f;
    double cycle = fmod(getTime() / max(0.1, TL), 0.22) / 0.22;
    double temperature = max(0.02, 0.10 * gs * pow(0.015, cycle));
    bool accepted = delta >= 0 || rnd() < exp(max(-700.0, double(delta) / temperature));
    double val = 0.1;
    if (ans.f > previousBest) {
      val = 12;
      previousBest = ans.f;
      lastBest = getTime();
    } else if (delta > 0)
      val = 5;
    else if (accepted)
      val = 1;
    destruction.val(d, val);
    reconstruction.val(r, val);

    if (iter % 3 == 0)
      for (int k = 0; k < K; k++)
        if (trial.a[k].a != current.a[k].a)
          save(k, trial.a[k]);
    if (accepted) {
      current = move(trial);
      for (int k = 0; k < K; k++)
        save(k, current.a[k]);
    }
    if (iter % 40 == 0) {
      destruction.update();
      reconstruction.update();
      learn(iter % 120 == 0 ? current : ans);
    }
    if (getTime() >= nextPack && !out(TL)) {
      const double packEnd = min(TL, getTime() + min(1.2, max(0.01, 0.01 * TL)));
      Sol packed;
      if (R > 120 && R <= 512 && K >= 8 && iter % 3 != 0)
        packed = globalPack(packEnd);
      else
        packed = pack(packEnd);
      if (packed.f > current.f || rnd() < 0.15)
        current = move(packed);
      if (!out(TL))
        ls(current, min(TL, getTime() + min(0.8, 0.005 * TL)));
      upd(current);
      nextPack = getTime() + min(5.0, max(0.06, 0.04 * TL));
    }
    if (stagnation > max(1.0, 0.12 * TL) && iter % 30 == 0) {
      current = rnd() < 0.7 ? ans : emptySol();
      if (current.f == 0)
        repair(current, 3, {}, min(TL, getTime() + slice), true);
    }
  }
  cerr << "event=alns_end t=" << getTime() << " best=" << ans.f << " stops=" << stops() << "\n";
}

// Writes the incumbent in the contest format after a last independent check.
void print() {
  if (!check(ans))
    throw logic_error("Final solution failed independent structural validation");
  cout << ans.f << '\n';
  for (const auto& route : ans.a) {
    size_t cnt = 0;
    for (int x : route.a)
      cnt += isP(x / 2) ? 2 : 1;
    cout << cnt;
    for (int x : route.a) {
      const auto& q = rq[x / 2];
      cout << ' ' << ((x & 1) ? q.d : q.p) + 1;
      if (isP(x / 2))
        cout << ' ' << q.d + 1;
    }
    cout << '\n';
  }
}

static void recoverAns() { // an exception escaped solve(): keep the best valid solution available
  bool ok = false;
  try {
    ok = check(ans);
  } catch (...) {
  }
  if (ok)
    return;
  try {
    if (sz(gSafe.a) == K && check(gSafe)) {
      swap(ans, gSafe);
      return;
    }
  } catch (...) {
  }
  try {
    ans = emptySol();
  } catch (...) {
  }
}

void init(double sec, uint64_t seed, bool use) {
  rng.seed(seed);
  st = clk::now();
  {
    const char* e = getenv("SISR_WARMIT");
    detClock = e && atof(e) > 0;
  }
  ex = use;
  TL = max(0.0, sec - min(0.15, sec * 0.02));
}
