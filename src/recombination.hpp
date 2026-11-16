#pragma once

// Recombination of archived routes.
//
//   pack        branch and bound over the archived routes of at most 12 taxis,
//               keeping the other taxis fixed
//   globalPack  simulated annealing over one archived route per taxi for the
//               whole fleet. It is used when 120 < R <= 512 and K >= 8.
//
// Both return the incumbent unless they find a strictly better plan, and every
// archived route is re-evaluated before it enters a plan.

// Set packing over archived routes. Adds one-request-removed variants of the
// incumbent routes to the archive, then searches combinations of at most 12
// taxis whose routes share no request.
Sol pack(double until) {
  Sol answer = ans, base = ans;
  vi taxis(K);
  iota(all(taxis), 0);
  if (K > 12) {
    shuffle(all(taxis), rng);
    taxis.resize(12);
  }
  double enrichEnd = getTime() + max(0.0, until - getTime()) * 0.12;
  for (int k : taxis) {
    vi ids;
    for (int x : ans.a[k].a)
      if (!(x & 1))
        ids.push_back(x / 2);
    shuffle(all(ids), rng);
    for (int j = 0; j < min(12, sz(ids)) && !out(enrichEnd); j++) {
      Rt smaller;
      for (int x : ans.a[k].a)
        if (x / 2 != ids[j])
          smaller.a.push_back(x);
      if (eval(k, smaller))
        save(k, smaller);
    }
  }
  for (int k : taxis) {
    save(k, ans.a[k]);
    base.a[k] = Rt();
  }
  syncSol(base);
  vc<char> used(R, 0);
  for (int r = 0; r < R; r++)
    used[r] = base.id[r] >= 0;
  vc<double> price(R, 0);
  for (int k = 0; k < K; k++) {
    int cnt = 0;
    for (int x : ans.a[k].a)
      cnt += !(x & 1);
    double share = 0.8 * double(ans.a[k].f) / max(1, cnt);
    for (int x : ans.a[k].a)
      price[x / 2] = share;
  }

  struct Choice {
    int k;
    vc<Col> cols;
  };

  vc<Choice> choices;
  for (int k : taxis) {
    Choice choice;
    choice.k = k;
    for (const auto& col : pool[k]) {
      bool ok = true;
      for (int r : col.ids)
        if (used[r]) {
          ok = false;
          break;
        }
      if (ok)
        choice.cols.push_back(col);
    }
    sort(all(choice.cols), [](const Col& a, const Col& b) { return a.f > b.f; });
    if (choice.cols.size() > 48) {
      vc<Col> diverse;
      vc<char> selected(choice.cols.size(), 0);
      for (int j = 0; j < 24; j++) {
        selected[j] = 1;
        diverse.push_back(choice.cols[j]);
      }
      vc<pair<double, int>> ranked;
      for (int j = 24; j < sz(choice.cols); j++) {
        const auto& col = choice.cols[j];
        ranked.emplace_back(-double(col.f) / double(col.ids.size()), j);
      }
      partial_sort(ranked.begin(), ranked.begin() + 8, ranked.end());
      for (int j = 0; j < 8; j++) {
        int id = ranked[j].second;
        selected[id] = 1;
        diverse.push_back(choice.cols[id]);
      }
      ranked.clear();
      for (int j = 24; j < sz(choice.cols); j++)
        if (!selected[j]) {
          double value = double(choice.cols[j].f);
          for (int r : choice.cols[j].ids)
            if (ans.id[r] >= 0 && ans.id[r] != k)
              value -= price[r];
          ranked.emplace_back(-value, j);
        }
      partial_sort(ranked.begin(), ranked.begin() + 16, ranked.end());
      for (int j = 0; j < 16; j++)
        diverse.push_back(choice.cols[ranked[j].second]);
      choice.cols = move(diverse);
      sort(all(choice.cols), [](const Col& a, const Col& b) { return a.f > b.f; });
    }

    const auto& incumbent = ans.a[k];
    if (incumbent.f > 0) {
      Col col;
      col.a = incumbent.a;
      col.f = incumbent.f;
      for (int x : col.a)
        if (!(x & 1))
          col.ids.push_back(x / 2);
      choice.cols.push_back(move(col));
      sort(all(choice.cols), [](const Col& a, const Col& b) { return a.f > b.f; });
    }
    choices.push_back(move(choice));
  }
  sort(all(choices),
       [](const Choice& a, const Choice& b) { return a.cols.size() < b.cols.size(); });
  int n = sz(choices);
  vl upper(n + 1, 0);
  for (int i = n - 1; i >= 0; i--)
    upper[i] = upper[i + 1] + (choices[i].cols.empty() ? 0 : choices[i].cols[0].f);
  vi selected(n, -1), winning;
  ll record = answer.f;
  uint64_t nodes = 0;
  function<void(int, ll)> dfs = [&](int d, ll value) {
    if (((++nodes & 255) == 0 && out(until)) || value + upper[d] <= record)
      return;
    if (d == n) {
      record = value;
      winning = selected;
      return;
    }
    if (out(until))
      return;
    for (int j = 0; j < sz(choices[d].cols); j++) {
      const auto& col = choices[d].cols[j];
      bool ok = true;
      for (int r : col.ids)
        if (used[r]) {
          ok = false;
          break;
        }
      if (!ok)
        continue;
      for (int r : col.ids)
        used[r] = 1;
      selected[d] = j;
      dfs(d + 1, value + col.f);
      for (int r : col.ids)
        used[r] = 0;
      if (out(until))
        break;
    }
    selected[d] = -1;
    dfs(d + 1, value);
  };
  dfs(0, base.f);
  if (!winning.empty()) {
    for (int d = 0; d < n; d++)
      if (winning[d] >= 0) {
        int k = choices[d].k;
        base.a[k].a = choices[d].cols[winning[d]].a;
        if (!eval(k, base.a[k]))
          throw logic_error("Invalid archived route");
      }
    syncSol(base);
    answer = move(base);
    upd(answer);
  }
  return answer;
}

// Fleet-wide recombination of already feasible taxi routes.  Request
// conflicts are resolved before a route can enter the working assignment.
using FleetMask = array<uint64_t, 8>;

struct FleetRoute {
  FleetMask mask{};
  const vi* path = nullptr;
  ll value = 0;
};

bool fleetOverlap(const FleetMask& a, const FleetMask& b) {
  for (size_t w = 0; w < a.size(); w++)
    if (a[w] & b[w])
      return true;
  return false;
}

void fleetAdd(FleetMask& a, const FleetMask& b) {
  for (size_t w = 0; w < a.size(); w++)
    a[w] |= b[w];
}

Sol globalPack(double until) {
  if (R <= 120 || R > 512 || K < 8 || out(until))
    return ans;
  Sol result = ans;
  vc<vc<FleetRoute>> routes(K);
  vi replaceable;
  for (int k = 0; k < K; k++) {
    if (out(until))
      return result;
    auto& row = routes[k];
    row.reserve(pool[k].size() + 2);
    row.push_back({});
    row.push_back({});
    row[1].path = &ans.a[k].a;
    row[1].value = ans.a[k].f;
    for (int x : ans.a[k].a)
      if (!(x & 1)) {
        int r = x / 2;
        row[1].mask[r >> 6] |= uint64_t(1) << (r & 63);
      }
    for (const auto& col : pool[k]) {
      if (col.a == ans.a[k].a)
        continue;
      FleetRoute z;
      z.path = &col.a;
      z.value = col.f;
      for (int r : col.ids)
        z.mask[r >> 6] |= uint64_t(1) << (r & 63);
      row.push_back(z);
    }
    sort(row.begin() + 2, row.end(),
         [](const FleetRoute& a, const FleetRoute& b) { return a.value > b.value; });
    if (sz(row) > 2)
      replaceable.push_back(k);
  }
  if (replaceable.empty())
    return result;
  vi current(K, 1), best = current, trial(K), pending;
  pending.reserve(K);
  ll score = ans.f, record = score;
  const double start = getTime();
  const double cooling = max(0.001, until - start);
  for (int it = 0; !out(until); it++) {
    if (it % 128 == 0) {
      current = best;
      score = record;
    }
    int a = replaceable[rd(sz(replaceable))];
    auto& row = routes[a];
    int cap = min(sz(row) - 2, it % 5 ? 32 : sz(row) - 2);
    int pick = 2 + rd(cap);
    if (pick == current[a])
      continue;
    trial = current;
    trial[a] = pick;
    FleetMask used = row[pick].mask;
    pending.clear();
    ll value = row[pick].value;
    for (int k = 0; k < K; k++)
      if (k != a) {
        const auto& z = routes[k][trial[k]];
        if (!trial[k] || fleetOverlap(row[pick].mask, z.mask) || rd(100) < 5) {
          trial[k] = 0;
          pending.push_back(k);
        } else {
          fleetAdd(used, z.mask);
          value += z.value;
        }
      }
    shuffle(all(pending), rng);
    for (int k : pending) {
      const auto& options = routes[k];
      int chosen = 0;
      if (!fleetOverlap(used, options[1].mask))
        chosen = 1;
      for (int i = 2; i < sz(options); i++) {
        if (options[i].value <= options[chosen].value)
          break;
        if (!fleetOverlap(used, options[i].mask)) {
          chosen = i;
          break;
        }
      }
      trial[k] = chosen;
      fleetAdd(used, options[chosen].mask);
      value += options[chosen].value;
    }
    const double age = min(1.0, (getTime() - start) / cooling);
    const double temperature = max(0.5, 0.4 * gs * (1.0 - age));
    if (value >= score || rnd() < exp(max(-700.0, double(value - score) / temperature))) {
      current.swap(trial);
      score = value;
    }
    if (score > record) {
      record = score;
      best = current;
    }
  }
  if (record <= result.f)
    return result;
  for (int k = 0; k < K; k++) {
    const auto* path = routes[k][best[k]].path;
    if (path)
      result.a[k].a = *path;
    else
      result.a[k].a.clear();
  }
  if (!check(result) || result.f != record)
    throw logic_error("Invalid fleet recombination");
  upd(result);
  return result;
}
