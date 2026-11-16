#pragma once

// Exact solver for tiny instances (R <= 10 and K <= 12).
//
// For each taxi, a depth-first search enumerates every ordering with dominance
// labels and keeps the best route for each served request set. A subset
// dynamic program then picks one set per taxi without sharing a request. If
// the search finishes in time the result is optimal and solve() returns early.

bool exactDfs(double until) {
  if (R > 10 || K > 12 || R == 0 || K == 0)
    return false;
  int S = 1 << R;
  // Relaxed travel times are lower bounds, including on nonmetric instances.
  vl lb = tmx;
  for (int h = 0; h < V; h++)
    for (int u = 0; u < V; u++)
      for (int v = 0; v < V; v++)
        lb[size_t(u) * V + v] =
            min(lb[size_t(u) * V + v], lb[size_t(u) * V + h] + lb[size_t(h) * V + v]);
  auto dist = [&](int u, int v) {
    return lb[size_t(u) * V + v];
  };
  vc<vl> value(K, vl(S, BAD));
  vc<vc<vi>> path(K, vc<vi>(S));
  map<pair<int, ll>, int> same;
  bool complete = true;
  for (int k = 0; k < K && !out(until); k++) {
    pair<int, ll> signature = {O[k], Q[k]};
    if (same.count(signature)) {
      int prev = same[signature];
      value[k] = value[prev];
      path[k] = path[prev];
      continue;
    }
    same[signature] = k;
    unordered_map<uint64_t, vc<pair<ll, ll>>> labels;
    vi route;
    uint64_t nodes = 0;
    function<void(int, int, int, ll, ll, ll)> dfs = [&](int picked, int done, int last, ll tm, ll w,
                                                        ll f) {
      if ((++nodes & 255) == 0 && out(until)) {
        complete = false;
        return;
      }
      if (!complete)
        return;
      int u = last < 0 ? O[k] : ev[last].v, possible = 0;
      ll optimistic = f;
      for (int r = 0; r < R; r++)
        if (!(done >> r & 1)) {
          const auto& q = rq[r];
          if (picked >> r & 1) {
            if (max(q.ed, tm + dist(u, q.d)) > q.ld)
              return;
            optimistic += q.rev;
          } else {
            ll at = max(q.e, tm + dist(u, q.p));
            if (q.w > Q[k] || at > q.l)
              continue;
            if (!isP(r) && max(q.ed, at + q.s + dist(q.p, q.d)) > q.ld)
              continue;
            possible |= 1 << r;
            optimistic += q.rev;
          }
        }
      // Requests are optional; a nonpositive complete route is never necessary.
      if (optimistic <= 0 && picked)
        return;
      uint64_t key = (uint64_t(picked) * S + done) * uint64_t(2 * R + 1) + (last + 1);
      auto& front = labels[key];
      for (auto p : front)
        if (p.first <= tm && p.second >= f)
          return;
      front.erase(remove_if(all(front), [&](auto p) { return tm <= p.first && f >= p.second; }),
                  front.end());
      front.emplace_back(tm, f);
      if (picked == done && f > value[k][done]) {
        value[k][done] = f;
        path[k][done] = route;
      }
      for (int r = 0; r < R; r++) {
        int bit = 1 << r, x;
        if (done & bit)
          continue;
        if (picked & bit) {
          if (isP(r))
            continue;
          x = 2 * r + 1;
        } else {
          if (!(possible & bit))
            continue;
          x = 2 * r;
        }
        const Node& b = ev[x];
        ll at = max(b.l, tm + t(u, b.u)), nextLoad = w + b.w;
        if (at > b.r || nextLoad < 0 || nextLoad > Q[k])
          continue;
        int np = picked | bit, nd = done;
        if (isP(r) || (x & 1))
          nd |= bit;
        route.push_back(x);
        dfs(np, nd, x, at + b.dt, nextLoad, f + b.val - b.ic - c(u, b.u));
        route.pop_back();
        if (!complete)
          return;
      }
    };
    dfs(0, 0, -1, 0, 0, 0);
    for (int mask = 1; mask < S; mask++)
      if (value[k][mask] > 0) {
        Rt z;
        z.a = path[k][mask];
        eval(k, z);
        save(k, z);
        Sol one = emptySol();
        one.a[k] = move(z);
        syncSol(one);
        upd(one);
      }
    if (!complete || out(until)) {
      complete = false;
      break;
    }
  }
  if (out(until))
    complete = false;
  if (!complete)
    return false;
  int joined = 0;
  vi choice(K);
  bool independent = true;
  for (int k = 0; k < K; k++) {
    choice[k] = int(max_element(all(value[k])) - value[k].begin());
    if (joined & choice[k])
      independent = false;
    joined |= choice[k];
  }
  if (independent) {
    Sol optimum = emptySol();
    for (int k = 0; k < K; k++) {
      optimum.a[k].a = path[k][choice[k]];
      if (!eval(k, optimum.a[k]))
        throw logic_error("Invalid exact route");
    }
    syncSol(optimum);
    upd(optimum);
    return true;
  }
  vc<vi> candidates(K);
  for (int k = 0; k < K; k++) {
    vl sub = value[k];
    for (int bit = 1; bit < S; bit <<= 1)
      for (int mask = 0; mask < S; mask++)
        if (mask & bit)
          sub[mask] = max(sub[mask], sub[mask ^ bit]);
    for (int mask = 1; mask < S; mask++)
      if (value[k][mask] > 0) {
        bool dominated = false;
        for (int bits = mask; bits; bits &= bits - 1)
          if (sub[mask ^ (bits & -bits)] >= value[k][mask]) {
            dominated = true;
            break;
          }
        if (dominated)
          value[k][mask] = BAD;
        else
          candidates[k].push_back(mask);
      }
  }
  vl dp(S, BAD);
  dp[0] = 0;
  vc<vi> parent(K, vi(S, -1));
  for (int k = 0; k < K; k++) {
    vl next = dp;
    for (int mask = 0; mask < S; mask++)
      if (dp[mask] != BAD) {
        if ((mask & 63) == 0 && out(until))
          return false;
        parent[k][mask] = 0;
      }
    for (int mask = 0; mask < S; mask++)
      if (dp[mask] != BAD) {
        if ((mask & 15) == 0 && out(until))
          return false;
        int remaining = (S - 1) ^ mask;
        auto relax = [&](int sub) {
          int dest = mask | sub;
          if (dp[mask] + value[k][sub] > next[dest]) {
            next[dest] = dp[mask] + value[k][sub];
            parent[k][dest] = sub;
          }
        };
        if (candidates[k].size() < (size_t(1) << __builtin_popcount(unsigned(remaining)))) {
          for (int sub : candidates[k])
            if (!(mask & sub))
              relax(sub);
        } else
          for (int sub = remaining; sub; sub = (sub - 1) & remaining)
            if (value[k][sub] > 0)
              relax(sub);
      }
    dp.swap(next);
  }
  int mask = int(max_element(all(dp)) - dp.begin());
  Sol optimum = emptySol();
  for (int k = K - 1; k >= 0; k--) {
    int sub = parent[k][mask];
    if (sub < 0)
      throw logic_error("Broken exact DP parent");
    optimum.a[k].a = path[k][sub];
    eval(k, optimum.a[k]);
    mask ^= sub;
  }
  syncSol(optimum);
  upd(optimum);
  return true;
}
