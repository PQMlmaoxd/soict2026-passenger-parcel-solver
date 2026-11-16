#pragma once

// Exact method for larger instances (R <= 512 and K <= 63).
//
// exactSparse() enumerates the feasible routes of each taxi under size and time
// caps. The enumeration prunes with shortest-path travel times as lower bounds,
// which stay valid on nonmetric input. Then it solves the set-packing master
// (each taxi at most one route, each request at most once):
//
//   reducedMaster       branch and bound with integer-scaled request prices
//   refineCliquePrices  projected subgradient refinement of the prices
//   stableClique        clique-strengthened relaxation with a bounded search
//
// A proof of optimality needs a complete pool. When a cap stops the
// enumeration, the routes found still feed the incumbent and the archive.

struct Mask {
  array<uint64_t, 8> w{};

  bool operator==(const Mask& x) const {
    return w == x.w;
  }

  bool hit(Mask x) const {
    for (int i = 0; i < 8; i++)
      if (w[i] & x.w[i])
        return true;
    return false;
  }

  bool sub(Mask x) const {
    for (int i = 0; i < 8; i++)
      if ((w[i] & x.w[i]) != w[i])
        return false;
    return true;
  }

  bool has(int r) const {
    return w[r >> 6] >> (r & 63) & 1;
  }

  Mask add(int r) const {
    Mask x = *this;
    x.w[r >> 6] |= 1ULL << (r & 63);
    return x;
  }

  Mask operator|(Mask x) const {
    for (int i = 0; i < 8; i++)
      x.w[i] |= w[i];
    return x;
  }
};

struct MH {
  size_t operator()(Mask x) const {
    uint64_t h = 0;
    for (auto v : x.w)
      h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
  }
};

struct XCol {
  Mask mask;
  ll f = 0, red = 0, pen = 0;
  vi a, ids;
};

struct MasterResult {
  vi choice;
  ll value;
  bool complete;
  uint64_t nodes;
  size_t kept;
};

MasterResult reducedMaster(const vc<XCol>& col, const vc<vi>& input, const vl& price,
                           const vi& incumbent, ll record, double until) {
  const ll scale = 1024;
  vc<vi> rows = input;
  vl red(col.size()), pen(col.size());
  for (size_t i = 0; i < col.size(); i++) {
    for (int r : col[i].ids)
      pen[i] += price[r];
    red[i] = col[i].f * scale - pen[i];
  }
  for (auto& row : rows)
    sort(all(row),
         [&](int a, int b) { return red[a] != red[b] ? red[a] > red[b] : col[a].f > col[b].f; });
  ll sumprice = accumulate(all(price), 0LL), root = sumprice;
  for (auto& row : rows)
    root += red[row.front()];
  size_t kept = 0;
  for (auto& row : rows) {
    ll top = red[row.front()];
    row.erase(
        remove_if(all(row), [&](int id) { return root - top + red[id] < (record + 1) * scale; }),
        row.end());
    kept += row.size();
  }
  vi current(K), best = incumbent;
  bool stopped = false;
  uint64_t nodes = 0;
  vc<char> spent(price.size(), 0);
  vc<uint64_t> reachable(price.size(), 0);
  uint64_t stamp = 0;
  function<void(uint64_t, Mask, ll, ll)> search = [&](uint64_t assigned, Mask used, ll value,
                                                      ll remaining) {
    if (stopped)
      return;
    ++nodes;
    if ((nodes & 127) == 1 && out(until)) {
      stopped = true;
      return;
    }
    if (assigned == ((1ULL << K) - 1)) {
      if (value > record) {
        record = value;
        best = current;
      }
      return;
    }
    vl tops(K, 0);
    __int128 upper = __int128(value) * scale + remaining;
    for (int k = 0; k < K; k++)
      if (!(assigned >> k & 1)) {
        bool ok = false;
        for (int id : rows[k])
          if (!used.hit(col[id].mask)) {
            tops[k] = red[id];
            ok = true;
            break;
          }
        if (!ok)
          return;
        upper += tops[k];
      }
    if (upper < (record + 1) * scale)
      return;
    // Refund dual prices that no remaining improving choice can consume.
    // This includes request prices and the memberships of conflict cliques.
    uint64_t mark = ++stamp;
    for (int k = 0; k < K; k++)
      if (!(assigned >> k & 1))
        for (int id : rows[k]) {
          if (upper - tops[k] + red[id] < (record + 1) * scale)
            break;
          if (!used.hit(col[id].mask))
            for (int p : col[id].ids)
              reachable[p] = mark;
        }
    for (size_t p = 0; p < price.size(); p++)
      if (!spent[p] && reachable[p] != mark)
        upper -= price[p];
    if (upper < (record + 1) * scale)
      return;
    int taxi = -1, countbest = INT_MAX;
    for (int k = 0; k < K; k++)
      if (!(assigned >> k & 1)) {
        int count = 0;
        for (int id : rows[k]) {
          if (upper - tops[k] + red[id] < (record + 1) * scale)
            break;
          if (!used.hit(col[id].mask))
            ++count;
        }
        if (!count)
          return;
        if (count < countbest) {
          countbest = count;
          taxi = k;
          if (count == 1)
            break;
        }
      }
    for (int id : rows[taxi]) {
      if (upper - tops[taxi] + red[id] < (record + 1) * scale)
        break;
      if (used.hit(col[id].mask))
        continue;
      current[taxi] = id;
      for (int p : col[id].ids)
        spent[p] = 1;
      search(assigned | (1ULL << taxi), used | col[id].mask, value + col[id].f,
             remaining - pen[id]);
      for (int p : col[id].ids)
        spent[p] = 0;
      if (stopped)
        return;
    }
  };
  search(0, {}, 0, sumprice);
  return {best, record, !stopped, nodes, kept};
}

vl refineCliquePrices(const vc<XCol>& col, const vc<vi>& rows, const vl& initial, ll lower,
                      double until) {
  const ll B = 1024;
  auto bound = [&](const vl& p) {
    ll upper = accumulate(all(p), 0LL);
    for (const auto& row : rows) {
      ll best = 0;
      for (int id : row) {
        ll v = col[id].f * B;
        for (int r : col[id].ids)
          v -= p[r];
        best = max(best, v);
      }
      upper += best;
    }
    return upper;
  };
  ll before = bound(initial);
  vc<double> prices(R), saved(R);
  for (int r = 0; r < R; r++)
    prices[r] = saved[r] = double(initial[r]) / B;
  double best = double(before) / B, alpha = 1.4;
  int stale = 0;
  ll total = 0;
  for (const auto& q : rq)
    total += q.rev;
  for (int it = 0; it < 2000 && !out(until); it++) {
    vi count(R);
    double upper = accumulate(all(prices), 0.0);
    for (const auto& row : rows) {
      double value = 0;
      int chosen = 0;
      for (int id : row) {
        double f = double(col[id].f);
        for (int r : col[id].ids)
          f -= prices[r];
        if (f > value) {
          value = f;
          chosen = id;
        }
      }
      upper += value;
      for (int r : col[chosen].ids)
        ++count[r];
    }
    if (upper < best - 1e-8) {
      best = upper;
      saved = prices;
      stale = 0;
    } else
      ++stale;
    double norm = 0;
    for (int r = 0; r < R; r++)
      if (prices[r] > 0 || count[r] > 1)
        norm += (count[r] - 1.0) * (count[r] - 1.0);
    if (norm < 1e-15 || upper <= double(lower) + 1e-8)
      break;
    if (stale >= 60) {
      alpha = max(.005, alpha * .7);
      prices = saved;
      stale = 0;
    }
    double step = alpha * max(0.0, upper - double(lower)) / norm;
    for (int r = 0; r < R; r++)
      prices[r] = min(double(total), max(0.0, prices[r] + step * (count[r] - 1)));
  }
  vl proposed(R);
  for (int r = 0; r < R; r++)
    proposed[r] = max(0LL, llround(saved[r] * B));
  ll after = bound(proposed);
  cerr << "event=dual_refine before=" << before / B << " after=" << after / B
       << " elapsed=" << getTime() << "\n";
  return after < before ? proposed : initial;
}

struct CliqueResult {
  vi choice;
  ll value;
  bool complete = false;
  int rounds = 0, cuts = 0;
  uint64_t nodes = 0;
};

CliqueResult stableClique(const vc<XCol>& col, const vc<vi>& rows, const vl& original,
                          const vi& incumbent, ll lower, double until, bool refineAllowed = true) {
  CliqueResult result;
  result.choice = incumbent;
  result.value = lower;
  const ll B = 1024;
  vl red(col.size());
  ll root = accumulate(all(original), 0LL);
  vl top(K);
  for (int k = 0; k < K; k++) {
    top[k] = 0;
    for (int id : rows[k]) {
      ll v = col[id].f * B;
      for (int r : col[id].ids)
        v -= original[r];
      red[id] = v;
      top[k] = max(top[k], v);
    }
    root += top[k];
  }
  if (root / B <= lower) {
    result.complete = true;
    return result;
  }
  vc<pair<int, int>> vars;
  for (int k = 0; k < K; k++)
    for (int id : rows[k])
      if (id && root - top[k] + red[id] >= (lower + 1) * B)
        vars.emplace_back(k, id);
  for (int k = 0; k < K; k++)
    if (incumbent[k] && find(vars.begin(), vars.end(), make_pair(k, incumbent[k])) == vars.end())
      vars.emplace_back(k, incumbent[k]);
  int n = sz(vars);
  // Bounded memory and work on unfamiliar instances; the caller retains R's
  // incumbent and fallback when this complete-pool path is unsuitable.
  if (n > 24000 && refineAllowed && !out(until)) {
    vl improved = refineCliquePrices(col, rows, original, lower, min(until, getTime() + 8.0));
    return stableClique(col, rows, improved, incumbent, lower, until, false);
  }
  if (n > 24000 || out(until))
    return result;
  vc<vi> cuts;
  set<vi> seen;
  vc<vc<uint64_t>> edges(n, vc<uint64_t>((n + 63) / 64));
  auto conflict = [&](int a, int b) {
    return (edges[a][b >> 6] >> (b & 63)) & 1ULL;
  };
  for (int i = 0; i < n; i++) {
    if ((i & 63) == 0 && out(until))
      return result;
    for (int j = 0; j < i; j++)
      if (vars[i].first == vars[j].first ||
          col[vars[i].second].mask.hit(col[vars[j].second].mask)) {
        edges[i][j >> 6] |= 1ULL << (j & 63);
        edges[j][i >> 6] |= 1ULL << (i & 63);
      }
  }
  vc<XCol> bestColumns;
  vc<vi> bestRows;
  vl bestPrices;
  vi bestInitial;
  ll bestBound = root;
  double lpEnd = min(until, getTime() + 30.0);
  for (int round = 0; round < 64 && !out(lpEnd); round++) {
    int m = K + R + sz(cuts), width = n + m + 1;
    if (size_t(m + 1) * width > 12000000)
      break;
    vc<double> table(size_t(m + 1) * width);
    vi basis(m);
    auto a = [&](int i, int j) -> double& {
      return table[size_t(i) * width + j];
    };
    for (int i = 0; i < m; i++) {
      basis[i] = n + i;
      a(i, n + i) = 1;
      a(i, width - 1) = 1 + 1e-6 * double(i + 1) / double(m + 1);
    }
    for (int j = 0; j < n; j++) {
      auto [k, id] = vars[j];
      a(k, j) = 1;
      for (int r : col[id].ids)
        a(K + r, j) = 1;
      a(m, j) = -double(col[id].f);
    }
    for (int h = 0; h < sz(cuts); h++)
      for (int j : cuts[h])
        a(K + R + h, j) = 1;
    // Start the primal basis at the already feasible incumbent. This avoids
    // rebuilding a good fleet assignment from the all-zero degenerate vertex.
    for (int j = 0; j < n; j++)
      if (vars[j].second == incumbent[vars[j].first]) {
        int leave = vars[j].first;
        double pivot = a(leave, j);
        if (abs(pivot) < 1e-9)
          continue;
        for (int v = 0; v < width; v++)
          a(leave, v) /= pivot;
        a(leave, j) = 1;
        for (int i = 0; i <= m; i++)
          if (i != leave) {
            double f = a(i, j);
            if (abs(f) < 1e-12) {
              a(i, j) = 0;
              continue;
            }
            for (int v = 0; v < width; v++)
              a(i, v) -= f * a(leave, v);
            a(i, j) = 0;
          }
        basis[leave] = j;
      }
    bool done = false;
    while (!out(lpEnd)) {
      int enter = -1;
      double lowest = -1e-7;
      for (int j = 0; j < n + m; j++)
        if (a(m, j) < lowest) {
          lowest = a(m, j);
          enter = j;
        }
      if (enter < 0) {
        done = true;
        break;
      }
      int leave = -1;
      double ratio = 1e100;
      for (int i = 0; i < m; i++)
        if (a(i, enter) > 1e-7) {
          double v = a(i, width - 1) / a(i, enter);
          if (v < ratio - 1e-12 ||
              (abs(v - ratio) < 1e-12 && (leave < 0 || basis[i] < basis[leave]))) {
            ratio = v;
            leave = i;
          }
        }
      if (leave < 0)
        break;
      double pivot = a(leave, enter);
      for (int j = 0; j < width; j++)
        a(leave, j) /= pivot;
      a(leave, enter) = 1;
      for (int i = 0; i <= m; i++)
        if (i != leave) {
          double f = a(i, enter);
          if (abs(f) < 1e-12) {
            a(i, enter) = 0;
            continue;
          }
          for (int j = 0; j < width; j++)
            a(i, j) -= f * a(leave, j);
          a(i, enter) = 0;
        }
      basis[leave] = enter;
    }
    vl prices(R + cuts.size());
    bool finite = true;
    for (int i = 0; i < sz(prices); i++) {
      double v = a(m, n + K + i) * B;
      if (!isfinite(v) || abs(v) > double(INF / (8 * (m + 1)))) {
        finite = false;
        break;
      }
      prices[i] = max(0LL, llround(v));
    }
    if (!finite)
      break;
    // Expand columns per taxi, appending cut memberships only to the price
    // indices. Feasibility masks remain the actual request masks. Because each
    // cut is a conflict clique, its price can be charged at most once.
    vc<XCol> expanded(1);
    vc<vi> expandedRows(K, vi{0});
    vi initial(K, 0);
    vc<unordered_map<int, int>> lookup(K);
    for (int j = 0; j < n; j++) {
      auto [k, id] = vars[j];
      int e = sz(expanded);
      expanded.push_back(col[id]);
      lookup[k][id] = e;
      expandedRows[k].push_back(e);
    }
    for (int h = 0; h < sz(cuts); h++)
      for (int j : cuts[h])
        expanded[j + 1].ids.push_back(R + h);
    for (int k = 0; k < K; k++)
      if (result.choice[k]) {
        auto it = lookup[k].find(result.choice[k]);
        if (it != lookup[k].end())
          initial[k] = it->second;
        else {
          initial[k] = sz(expanded);
          expanded.push_back(col[result.choice[k]]);
        }
      }
    vl taxiPrice(K);
    ll bound = accumulate(all(prices), 0LL);
    for (int k = 0; k < K; k++) {
      for (int j : expandedRows[k]) {
        ll v = expanded[j].f * B;
        for (int r : expanded[j].ids)
          v -= prices[r];
        taxiPrice[k] = max(taxiPrice[k], v);
      }
      bound += taxiPrice[k];
    }
    ++result.rounds;
    result.cuts = sz(cuts);
    vc<double> x(n);
    vi support;
    for (int i = 0; i < m; i++)
      if (basis[i] < n && a(i, width - 1) > 1e-7) {
        x[basis[i]] = a(i, width - 1);
        support.push_back(basis[i]);
      }
    vc<vi> supportRows(K, vi{0});
    for (int j : support)
      supportRows[vars[j].first].push_back(j + 1);
    for (int k = 0; k < K; k++)
      supportRows[k].push_back(initial[k]);
    for (auto& row : supportRows) {
      sort(all(row));
      row.erase(unique(all(row)), row.end());
    }
    auto local = reducedMaster(expanded, supportRows, prices, initial, result.value,
                               min(lpEnd, getTime() + 0.15));
    result.nodes += local.nodes;
    if (local.value > result.value) {
      result.value = local.value;
      for (int k = 0; k < K; k++) {
        int j = local.choice[k];
        result.choice[k] = j == 0 ? 0 : j <= n ? vars[j - 1].second : result.choice[k];
      }
    }
    if (bound / B <= result.value) {
      result.complete = true;
      return result;
    }
    // A stronger incumbent makes global reduced-cost pruning much tighter.
    // Return it so the caller can rebuild a smaller relaxation immediately.
    if (result.value > lower)
      return result;
    if (bound < bestBound) {
      bestBound = bound;
      bestColumns = move(expanded);
      bestRows = move(expandedRows);
      bestPrices = move(prices);
    }
    if (!done)
      break;
    sort(all(support), [&](int i, int j) { return x[i] > x[j]; });
    vc<pair<double, vi>> proposals;
    for (int s : support) {
      if (out(lpEnd))
        break;
      for (int t : support)
        if (s < t && conflict(s, t)) {
          vi clique{s, t};
          double mass = x[s] + x[t];
          for (int j : support)
            if (j != s && j != t) {
              bool ok = true;
              for (int v : clique)
                if (!conflict(j, v)) {
                  ok = false;
                  break;
                }
              if (ok) {
                clique.push_back(j);
                mass += x[j];
              }
            }
          sort(all(clique));
          if (mass > 1 + 1e-7 && seen.insert(clique).second)
            proposals.emplace_back(mass - 1, move(clique));
        }
    }
    if (proposals.empty())
      break;
    sort(all(proposals), [](const auto& a, const auto& b) {
      if (a.first != b.first)
        return a.first > b.first;
      return a.second < b.second;
    });
    for (int i = 0; i < min(24, sz(proposals)); i++)
      cuts.push_back(move(proposals[i].second));
  }
  if (!bestColumns.empty() && !out(until)) {
    vi initial(K, 0);
    vi source(bestColumns.size(), -1);
    for (int j = 0; j < n; j++)
      source[j + 1] = vars[j].second;
    for (int k = 0; k < K; k++)
      if (result.choice[k]) {
        for (int j : bestRows[k])
          if (source[j] == result.choice[k]) {
            initial[k] = j;
            break;
          }
        if (!initial[k]) {
          initial[k] = sz(bestColumns);
          source.push_back(result.choice[k]);
          bestColumns.push_back(col[result.choice[k]]);
        }
      }
    auto exact = reducedMaster(bestColumns, bestRows, bestPrices, initial, result.value, until);
    result.nodes += exact.nodes;
    if (exact.value > result.value) {
      result.value = exact.value;
      for (int k = 0; k < K; k++)
        result.choice[k] = exact.choice[k] ? source[exact.choice[k]] : 0;
    }
    result.complete = exact.complete;
  }
  return result;
}

bool exactSparse(double until) {
  if (R > 512 || K > 63 || !R || !K || out(until))
    return false;
  ll total = 0;
  for (auto q : rq) {
    if (q.rev > INF / 1048576 - total)
      return false;
    total += q.rev;
  }
  double begin = getTime(), genEnd = begin + 0.65 * (until - begin);
  if (R > 256 || K > 24)
    genEnd = min(genEnd, begin + 20.0);
  vl lb = tmx;
  for (int h = 0; h < V; h++) {
    if (out(genEnd))
      return false;
    for (int u = 0; u < V; u++)
      for (int v = 0; v < V; v++)
        lb[size_t(u) * V + v] =
            min(lb[size_t(u) * V + v], lb[size_t(u) * V + h] + lb[size_t(h) * V + v]);
  }
  auto dist = [&](int u, int v) {
    return lb[size_t(u) * V + v];
  };
  vc<XCol> col(1);
  vc<vi> by(K, vi(1, 0));
  vi state(R, 0), path;
  bool complete = true;
  map<pair<int, ll>, int> same;
  for (int k = 0; k < K; k++) {
    pair<int, ll> key = {O[k], Q[k]};
    if (same.count(key)) {
      by[k] = by[same[key]];
      continue;
    }
    same[key] = k;
    unordered_map<Mask, int, MH> index;
    ll localBest = 0;
    uint64_t nodes = 0;
    function<void(int, ll, ll, int, Mask, ll)> dfs = [&](int u, ll tm, ll w, int open, Mask mask,
                                                         ll f) {
      if (!complete)
        return;
      if ((++nodes & 255) == 0 && (out(genEnd) || col.size() > 250000 || index.size() > 30000)) {
        complete = false;
        return;
      }
      if (!open && f > 0) {
        auto it = index.find(mask);
        if (it == index.end()) {
          int id = sz(col);
          index[mask] = id;
          XCol z;
          z.mask = mask;
          z.f = f;
          z.a = path;
          for (int x : path)
            if (!(x & 1))
              z.ids.push_back(x / 2);
          col.push_back(move(z));
          by[k].push_back(id);
        } else if (f > col[it->second].f) {
          col[it->second].f = f;
          col[it->second].a = path;
        }
        if (f > localBest) {
          localBest = f;
          Sol one = emptySol();
          one.a[k].a = path;
          if (!eval(k, one.a[k]))
            throw 0;
          syncSol(one);
          upd(one);
        }
      }
      ll optimistic = f;
      Mask possible;
      for (int r = 0; r < R; r++)
        if (state[r] != 2) {
          const auto& q = rq[r];
          if (state[r] == 1) {
            if (max(q.ed, tm + dist(u, q.d)) > q.ld)
              return;
            optimistic += q.rev;
          } else {
            ll at = max(q.e, tm + dist(u, q.p));
            if (q.w > Q[k] || at > q.l)
              continue;
            if (!isP(r) && max(q.ed, at + q.s + dist(q.p, q.d)) > q.ld)
              continue;
            possible = possible.add(r);
            optimistic += q.rev;
          }
        }
      if (optimistic <= 0)
        return;
      for (int r = 0; r < R; r++) {
        int old = state[r];
        if (old == 2 || (!old && !possible.has(r)))
          continue;
        int x = 2 * r + (old == 1);
        const auto& b = ev[x];
        ll at = max(b.l, tm + t(u, b.u)), nw = w + b.w;
        if (at > b.r || nw < 0 || nw > Q[k])
          continue;
        state[r] = isP(r) || old == 1 ? 2 : 1;
        path.push_back(x);
        dfs(b.v, at + b.dt, nw,
            open + (isP(r)     ? 0
                    : old == 1 ? -1
                               : 1),
            mask.add(r), f + b.val - b.ic - c(u, b.u));
        path.pop_back();
        state[r] = old;
        if (!complete)
          return;
      }
    };
    dfs(O[k], 0, 0, 0, {}, 0);
    if (out(genEnd))
      complete = false;
    if (!complete)
      break;
  }
  for (auto& row : by)
    sort(all(row), [&](int a, int b) { return col[a].f > col[b].f; });
  vi chosen(K, 0), bestChoice(K, 0);
  ll record = 0;
  auto accept = [&](const vi& a) {
    Mask used;
    ll value = 0;
    for (int id : a) {
      if (used.hit(col[id].mask))
        return;
      used = used | col[id].mask;
      value += col[id].f;
    }
    if (value > record) {
      record = value;
      bestChoice = a;
    }
  };
  {
    vi base(K, 0);
    for (int k = 0; k < K; k++) {
      Mask m;
      for (int x : ans.a[k].a)
        if (!(x & 1))
          m = m.add(x / 2);
      for (int id : by[k])
        if (col[id].mask == m) {
          base[k] = id;
          break;
        }
    }
    accept(base);
  }
  auto publish = [&]() {
    if (record <= ans.f)
      return;
    Sol s = emptySol();
    for (int k = 0; k < K; k++) {
      s.a[k].a = col[bestChoice[k]].a;
      if (!eval(k, s.a[k]))
        throw 0;
    }
    syncSol(s);
    upd(s);
  };
  auto archive = [&]() {
    for (int k = 0; k < K && !out(until); k++)
      for (int j = 0; j < min(12, sz(by[k])); j++) {
        Rt route;
        route.a = col[by[k][j]].a;
        if (eval(k, route))
          save(k, route);
      }
  };
  vc<double> prices(R, 0), saved(R, 0);
  auto greedy = [&](int tries) {
    vi order(K);
    iota(all(order), 0);
    for (int it = 0; it < tries && !out(until); it++) {
      shuffle(all(order), rng);
      Mask used;
      double factor = rnd();
      for (int k : order) {
        double best = -1e100;
        int take = 0;
        for (int id : by[k]) {
          if (double(col[id].f) <= best)
            break;
          if (used.hit(col[id].mask))
            continue;
          double value = double(col[id].f);
          for (int r : col[id].ids)
            value -= factor * prices[r];
          if (value > best) {
            best = value;
            take = id;
          }
        }
        chosen[k] = take;
        used = used | col[take].mask;
      }
      accept(chosen);
    }
  };
  greedy(100);
  publish();
  // An interrupted enumeration provides feasible columns, never an exact proof.
  if (!complete || out(until)) {
    archive();
    return false;
  }
  for (int k = 0; k < K; k++) {
    vi keep;
    for (int id : by[k]) {
      bool dominated = false;
      if (by[k].size() <= 2000 && !out(until))
        for (int other : by[k])
          if (id != other && col[other].f >= col[id].f && col[other].mask.sub(col[id].mask)) {
            dominated = true;
            break;
          }
      if (!dominated)
        keep.push_back(id);
    }
    by[k] = move(keep);
  }
  bool large = R > 256 || K > 24;
  double bestDual = 1e100, alpha = 1.8;
  double dualEnd =
      min(until, getTime() + min(large ? 20.0 : 0.5, (large ? 0.35 : 0.15) * (until - begin)));
  int stale = 0;
  for (int it = 0; it < 5000 && !out(dualEnd); it++) {
    vi count(R, 0);
    double upper = accumulate(all(prices), 0.0);
    for (int k = 0; k < K; k++) {
      double best = -1e100;
      int take = 0;
      for (int id : by[k]) {
        double value = double(col[id].f);
        for (int r : col[id].ids)
          value -= prices[r];
        if (value > best) {
          best = value;
          take = id;
        }
      }
      upper += best;
      chosen[k] = take;
      for (int r : col[take].ids)
        count[r]++;
    }
    accept(chosen);
    if (upper < bestDual - 1e-7) {
      bestDual = upper;
      saved = prices;
      stale = 0;
    } else
      stale++;
    double norm = 0;
    for (int r = 0; r < R; r++)
      if (prices[r] > 0 || count[r] > 1)
        norm += (count[r] - 1.0) * (count[r] - 1.0);
    if (norm < 1e-15 || upper - double(record) < 1e-7)
      break;
    if (it % (large ? 20 : 100) == 0)
      greedy(8);
    if (stale >= 100) {
      alpha = max(0.002, alpha * 0.75);
      prices = saved;
      stale = 0;
    }
    double step = alpha * max(0.0, upper - double(record)) / norm;
    for (int r = 0; r < R; r++)
      prices[r] = min(double(total), max(0.0, prices[r] + step * (count[r] - 1)));
  }
  // Quantize the dual to integers; all final B&B bounds are exact integer bounds.
  const ll B = 1024;
  vl price(R, 0);
  for (int r = 0; r < R; r++)
    price[r] = max(0LL, llround(saved[r] * B));
  for (auto& z : col) {
    for (int r : z.ids)
      z.pen += price[r];
    z.red = z.f * B - z.pen;
  }
  vc<vi> profitRows;
  if (large)
    profitRows = by;
  for (auto& row : by)
    sort(all(row), [&](int a, int b) { return col[a].red > col[b].red; });
  archive();
  ll root = accumulate(all(price), 0LL);
  for (const auto& row : by)
    root += col[row[0]].red;
  if (root / B <= record) {
    publish();
    return true;
  }
  if (large) {
    double end = min(until, getTime() + min(80.0, 0.7 * (until - begin)));
    vi current = bestChoice;
    ll value = record;
    uint64_t it = 0;
    while (!out(end)) {
      int k = int(rng() % unsigned(K));
      const auto& row = by[k];
      int width = min(sz(row), 32 + int(it % 224));
      int id = row[size_t(rng() % unsigned(width))];
      vi trial = current, repairOrder;
      Mask used = col[id].mask;
      trial[k] = id;
      for (int j = 0; j < K; j++)
        if (j != k) {
          if (used.hit(col[trial[j]].mask) || rnd() < 0.06) {
            trial[j] = 0;
            repairOrder.push_back(j);
          } else
            used = used | col[trial[j]].mask;
        }
      shuffle(all(repairOrder), rng);
      for (int j : repairOrder) {
        for (int x : profitRows[j])
          if (!used.hit(col[x].mask)) {
            trial[j] = x;
            used = used | col[x].mask;
            break;
          }
      }
      ll next = 0;
      for (int x : trial)
        next += col[x].f;
      double temperature = max(1.0, 0.025 * double(record) / K);
      if (next >= value || rnd() < exp(double(next - value) / temperature)) {
        current = trial;
        value = next;
      }
      accept(trial);
      if (++it % 10000 == 0) {
        current = bestChoice;
        value = record;
      }
    }
    publish();
  }
  if (large && !out(TL - 5.0)) {
    auto start = getTime();
    double end = min(TL - 5.0, start + 175.0);
    while (!out(end)) {
      ll previous = record;
      auto z = stableClique(col, by, price, bestChoice, record, end);
      accept(z.choice);
      publish();
      if (record != z.value)
        throw logic_error("Clique master objective mismatch");
      cerr << "event=stable_master benefit=" << record << " proved=" << z.complete
           << " rounds=" << z.rounds << " cuts=" << z.cuts << " nodes=" << z.nodes
           << " seconds=" << getTime() - start << " elapsed=" << getTime() << "\n";
      if (z.complete)
        return true;
      if (record <= previous)
        break;
    }
  }
  double branchEnd = until;
  if (large)
    branchEnd = min(until, getTime() + min(3.0, 0.1 * (until - begin)));
  bool stopped = false;
  uint64_t nodes = 0;
  function<void(uint64_t, Mask, ll, ll)> branch = [&](uint64_t assigned, Mask used, ll value,
                                                      ll remaining) {
    if (stopped)
      return;
    if ((++nodes & 255) == 0 && out(branchEnd)) {
      stopped = true;
      return;
    }
    if (assigned == (1ULL << K) - 1) {
      if (value > record)
        accept(chosen);
      return;
    }
    vl top(K, 0);
    ll upper = value * B + remaining;
    for (int k = 0; k < K; k++)
      if (!(assigned >> k & 1)) {
        for (int id : by[k])
          if (!used.hit(col[id].mask)) {
            top[k] = col[id].red;
            break;
          }
        upper += top[k];
      }
    if (upper / B <= record)
      return;
    int taxi = -1, least = INT_MAX;
    for (int k = 0; k < K; k++)
      if (!(assigned >> k & 1)) {
        int count = 0;
        for (int id : by[k]) {
          if (upper - top[k] + col[id].red < (record + 1) * B)
            break;
          count += !used.hit(col[id].mask);
        }
        if (count < least) {
          least = count;
          taxi = k;
        }
      }
    if (!least)
      return;
    for (int id : by[taxi]) {
      if (upper - top[taxi] + col[id].red < (record + 1) * B)
        break;
      const auto& z = col[id];
      if (used.hit(z.mask))
        continue;
      chosen[taxi] = id;
      branch(assigned | (1ULL << taxi), used | z.mask, value + z.f, remaining - z.pen);
      if (stopped)
        return;
    }
  };
  if (!out(until))
    branch(0, {}, 0, accumulate(all(price), 0LL));
  else
    stopped = true;
  publish();
  return !stopped;
}
