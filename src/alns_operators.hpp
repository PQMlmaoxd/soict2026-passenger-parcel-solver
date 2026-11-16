#pragma once

// Destroy and repair operators of the ALNS phase.
//
// repair() has eight modes, ruin() has seven. They are numbered in the order
// of the operator tables in docs/algorithm.md:
//
//   repair  0 greedy, 1 regret-2, 2 regret-3, 3 noisy greedy, 4 affinity,
//           5 exploratory, 6 delay ratio, 7 delay penalty
//   ruin    0 random, 1 related, 2 best removal gain, 3 route removal,
//           4 parcel-biased related, 5 route segment, 6 pickup deadline

// Unserved requests to reconsider: first the priority list (the ones just
// removed), then up to `cap` others, two thirds by noisy revenue rank and the
// rest at random.
vi reqList(const Sol& s, const vi& priority, int cap) {
  vi ids;
  vc<char> seen(R, 0);
  for (int r : priority)
    if (s.id[r] < 0 && !seen[r]) {
      ids.push_back(r);
      seen[r] = 1;
    }
  vc<pair<double, int>> others;
  for (int r = 0; r < R; r++)
    if (s.id[r] < 0 && !seen[r])
      others.emplace_back(-(double(rq[r].rev) + gs * rnd() * 2), r);
  int take = max(0, cap - sz(ids));
  take = min(take, sz(others));

  int ranked = take == sz(others) ? take : (2 * take / 3);
  partial_sort(others.begin(), others.begin() + ranked, others.end());
  for (int j = 0; j < ranked; j++)
    ids.push_back(others[j].second);
  shuffle(others.begin() + ranked, others.end(), rng);
  for (int j = ranked; j < take; j++)
    ids.push_back(others[j].second);
  shuffle(all(ids), rng);
  return ids;
}

// Reinserts unserved requests one at a time. Each round scores the best move of
// every candidate request according to `mode` and commits the highest score.
// Move gains are cached per (request, taxi) and refreshed when a route changes.
void repair(Sol& s, int mode, const vi& priority, double until, bool full = false,
            const vi* oldOwner = nullptr) {
  int cap = full ? R : (R <= 120 ? R : 96);
  auto ids = reqList(s, priority, cap);

  struct Cache {
    vi taxis, version;
    vc<Ins> moves;
  };

  vc<Cache> cache(ids.size());
  vi version(K, 0);
  for (size_t z = 0; z < ids.size(); z++) {
    cache[z].taxis = taxiList(s, ids[z], oldOwner ? (*oldOwner)[ids[z]] : -1);
    cache[z].version.assign(cache[z].taxis.size(), -1);
    cache[z].moves.resize(cache[z].taxis.size());
  }
  int explorations = 0;
  while (!out(until)) {
    double potential = gs;
    if (mode == 5)
      for (int r : ids)
        if (s.id[r] < 0)
          potential += double(rq[r].rev);
    int chosen = -1;
    Ins chosenMove;
    double bestScore = -1e300;
    for (size_t z = 0; z < ids.size() && !out(until); z++) {
      int r = ids[z];
      if (s.id[r] >= 0)
        continue;
      auto& cc = cache[z];
      vc<Ins> options;
      for (size_t j = 0; j < cc.taxis.size() && !out(until); j++) {
        int k = cc.taxis[j];
        if (cc.version[j] != version[k]) {
          cc.moves[j] = findMove(s, r, k, until);
          cc.version[j] = version[k];
        }
        if (cc.moves[j].ok())
          options.push_back(cc.moves[j]);
      }
      if (options.empty())
        continue;
      sort(all(options), [](const Ins& a, const Ins& b) { return a.gain > b.gain; });
      Ins p = options[0];
      if (mode == 4) {
        double mx = -1e300;
        for (auto v : options)
          if (v.gain > 0) {
            double sc = double(v.gain) + 0.15 * gs * log(max(0.02, tau(r, v.k)));
            if (sc > mx) {
              mx = sc;
              p = v;
            }
          }
      }
      if (p.gain <= 0 &&
          (mode != 5 || explorations >= (s.f <= 0 ? 8 : 2) || double(p.gain) < -potential))
        continue;
      double scr = double(p.gain);
      if (mode == 1 || mode == 2) {
        int depth = mode == 1 ? 2 : 3;

        double regret = 0;
        for (int j = 1; j < depth; j++)
          regret += double(p.gain - (j < sz(options) ? max<ll>(0, options[j].gain) : 0));
        scr += 0.7 * regret;
      }
      if (mode == 3 || mode == 5)
        scr += (rnd() - 0.5) * gs * 0.5;
      if (mode == 4)
        scr += 0.15 * gs * log(max(0.02, tau(r, p.k)));
      if (mode == 6)
        scr /= 1.0 + 0.45 * double(p.delay) / gt;
      if (mode == 7)
        scr -= 0.20 * gs * double(p.delay) / gt;
      if (scr > bestScore) {
        bestScore = scr;
        chosen = r;
        chosenMove = p;
      }
    }
    if (chosen < 0 || out(until))
      break;
    if (chosenMove.gain <= 0)
      explorations++;
    put(s, chosen, chosenMove);
    version[chosenMove.k]++;
    if (K > 24 && mode == 5)
      for (auto& cc : cache)
        if (find(all(cc.taxis), chosenMove.k) == cc.taxis.end()) {
          cc.taxis.push_back(chosenMove.k);
          cc.version.push_back(-1);
          cc.moves.push_back(Ins());
        }
    upd(s);
  }
}

vi ruin(Sol& s, int mode, int amount, double until) {
  auto ids = used(s);
  if (ids.empty())
    return {};
  int anchor = ids[rd(sz(ids))];
  vc<pair<double, int>> rank;
  if (mode == 0)
    shuffle(all(ids), rng);
  else if (mode == 1 || mode == 4) {
    for (int r : ids) {
      double d = double(t(rq[anchor].p, rq[r].p)) + double(t(rq[anchor].d, rq[r].d)) +
                 0.3 * abs(double(rq[anchor].e) - double(rq[r].e));
      if (s.id[r] != s.id[anchor])
        d += 0.3 * gs;
      if (mode == 4 && isP(r))
        d += gs;
      rank.emplace_back(d * (0.8 + 0.4 * rnd()), r);
    }
  } else if (mode == 2) {
    for (int r : ids) {
      if (out(until))
        break;
      int k = s.id[r];
      Rt z;
      for (int x : s.a[k].a)
        if (x / 2 != r)
          z.a.push_back(x);
      double benefit = eval(k, z) ? double(z.f - s.a[k].f) : -gs * 10;
      rank.emplace_back(-benefit + rnd() * gs * 0.1, r);
    }
  } else if (mode == 3) {
    int k = s.id[anchor];
    for (int r : ids)
      rank.emplace_back((s.id[r] == k ? 0.0 : 1.0) + rnd(), r);
    amount = max(amount, int(count_if(all(ids), [&](int r) { return s.id[r] == k; })));
  } else if (mode == 5) {
    const auto& a = s.a[s.id[anchor]].a;
    int at = rd(sz(a));
    vi position(R, -1000000);
    for (int i = 0; i < sz(a); i++)
      position[a[i] / 2] = i;
    for (int r : ids)
      rank.emplace_back(abs(position[r] - at) + rnd(), r);
  } else {
    for (int r : ids)
      rank.emplace_back(abs(double(rq[r].l) - double(rq[anchor].l)) + rnd() * gs, r);
  }
  if (!rank.empty()) {
    sort(all(rank));
    ids.clear();
    for (auto x : rank)
      ids.push_back(x.second);
  }
  ids.resize(min(amount, sz(ids)));
  auto before = s.id;
  eraseReq(s, ids);

  ids.clear();
  for (int r = 0; r < R; r++)
    if (before[r] >= 0 && s.id[r] < 0)
      ids.push_back(r);
  return ids;
}
