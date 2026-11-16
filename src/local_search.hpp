#pragma once

// Local search applied to a plan during ALNS, and the affinity update.
//
//   optRoute   relocate one event or reverse a segment inside one route
//   crossTail  swap route tails between two taxis, only at cuts where no parcel
//              is on board in either route
//   optBlock   exact reordering of a window of up to 11 events by a
//              label-setting search over subsets, keeping precedence and load
//   ls         runs the pieces above under a shared time budget
//   learn      moves the affinity tau(r, k) toward the taxi that serves r in an
//              elite plan

bool optRoute(Sol& s, int k, double until) {
  bool improved = false;
  for (int pass = 0; pass < 2 && !out(until); pass++) {
    const auto& z = s.a[k];
    const auto& a = z.a;
    int L = sz(a);
    if (L < 2)
      return improved;
    vi position(2 * R, -1), order(L);
    for (int i = 0; i < L; i++)
      position[a[i]] = i;
    iota(all(order), 0);
    shuffle(all(order), rng);
    ll gain = 0;
    Rt winner;
    for (int i : order) {
      if (out(until))
        break;
      int x = a[i], u = i ? ev[a[i - 1]].v : O[k];
      ll saving = c(u, ev[x].u);
      if (i + 1 < L)
        saving += c(ev[x].v, ev[a[i + 1]].u) - c(u, ev[a[i + 1]].u);
      int offset = rd(L + 1);
      for (int h = 0; h <= L; h++) {
        int j = (h + offset) % (L + 1);
        if ((h & 31) == 0 && out(until))
          break;
        if (j == i || j == i + 1)
          continue;
        if (!isP(x / 2)) {
          if (!(x & 1) && j > position[x + 1])
            continue;
          if ((x & 1) && j <= position[x - 1])
            continue;
        }
        int prev = j ? ev[a[j - 1]].v : O[k];
        ll extra = c(prev, ev[x].u);
        if (j < L)
          extra += c(ev[x].v, ev[a[j]].u) - c(prev, ev[a[j]].u);
        if (saving - extra <= gain)
          continue;
        Rt candidate;
        candidate.a = a;
        candidate.a.erase(candidate.a.begin() + i);
        candidate.a.insert(candidate.a.begin() + j - (i < j ? 1 : 0), x);
        if (eval(k, candidate)) {
          gain = candidate.f - z.f;
          winner = move(candidate);
        }
      }
    }
    vl reverseDelta(L, 0);
    for (int i = 1; i < L; i++)
      reverseDelta[i] =
          reverseDelta[i - 1] + c(ev[a[i]].v, ev[a[i - 1]].u) - c(ev[a[i - 1]].v, ev[a[i]].u);
    for (int i : order) {
      if (out(until))
        break;
      int prev = i ? ev[a[i - 1]].v : O[k];
      for (int j = i + 1; j < L; j++) {
        if ((j & 31) == 0 && out(until))
          break;
        ll saving = c(prev, ev[a[i]].u) - c(prev, ev[a[j]].u) - (reverseDelta[j] - reverseDelta[i]);
        if (j + 1 < L)
          saving += c(ev[a[j]].v, ev[a[j + 1]].u) - c(ev[a[i]].v, ev[a[j + 1]].u);
        if (saving <= gain)
          continue;
        bool ok = true;
        for (int h = i; h <= j; h++)
          if (!isP(a[h] / 2) && !(a[h] & 1) && position[a[h] + 1] <= j) {
            ok = false;
            break;
          }
        if (!ok)
          continue;
        Rt candidate;
        candidate.a = a;
        reverse(candidate.a.begin() + i, candidate.a.begin() + j + 1);
        if (eval(k, candidate)) {
          gain = candidate.f - z.f;
          winner = move(candidate);
        }
      }
    }
    if (gain <= 0)
      break;
    s.f += gain;
    s.a[k] = move(winner);
    improved = true;
    save(k, s.a[k]);
    upd(s);
  }
  return improved;
}

void crossTail(Sol& s, double until) {
  if (K < 2)
    return;
  for (int rep = 0; rep < min(24, 2 * K) && !out(until); rep++) {
    int a = rd(K), b = rd(K);
    if (a == b)
      continue;
    const auto& x = s.a[a].a;
    const auto& y = s.a[b].a;
    auto cuts = [&](const vi& route) {
      vi points = {0};
      int open = 0;
      for (int i = 0; i < sz(route); i++) {
        int event = route[i];
        if (!isP(event / 2))
          open += (event & 1) ? -1 : 1;
        if (!open)
          points.push_back(i + 1);
      }
      shuffle(all(points), rng);
      return points;
    };
    auto first = cuts(x), second = cuts(y);
    ll gain = 0;
    Rt winA, winB;
    for (int i : first) {
      if (out(until))
        break;
      int u = i ? ev[x[i - 1]].v : O[a];
      for (int j : second) {
        if (out(until))
          break;
        int v = j ? ev[y[j - 1]].v : O[b];
        ll delta = 0;
        if (i < sz(x))
          delta += c(u, ev[x[i]].u) - c(v, ev[x[i]].u);
        if (j < sz(y))
          delta += c(v, ev[y[j]].u) - c(u, ev[y[j]].u);
        if (delta <= gain)
          continue;
        Rt na, nb;
        na.a.insert(na.a.end(), x.begin(), x.begin() + i);
        na.a.insert(na.a.end(), y.begin() + j, y.end());
        nb.a.insert(nb.a.end(), y.begin(), y.begin() + j);
        nb.a.insert(nb.a.end(), x.begin() + i, x.end());
        if (eval(a, na) && eval(b, nb)) {
          gain = delta;
          winA = move(na);
          winB = move(nb);
        }
      }
    }
    if (gain > 0) {
      s.a[a] = move(winA);
      s.a[b] = move(winB);
      syncSol(s);
      save(a, s.a[a]);
      save(b, s.a[b]);
      upd(s);
    }
  }
}

bool optBlock(Sol& s, int k, int begin, int length, double until) {
  if (k < 0 || k >= K || begin < 0 || out(until))
    return false;
  const Rt& original = s.a[k];
  int routeLength = sz(original.a);
  if (begin >= routeLength)
    return false;
  length = min({length, 11, routeLength - begin});
  if (length < 2)
    return false;
  int end = begin + length;
  vi events(original.a.begin() + begin, original.a.begin() + end);
  int u0 = begin ? ev[original.a[begin - 1]].v : O[k];
  ll t0 = begin ? original.tm[begin - 1] : 0;
  ll w0 = begin ? original.w[begin - 1] : 0;
  int nxt = end < routeLength ? original.a[end] : -1;

  ll oldCost = 0;
  int previous = u0;
  for (int x : events) {
    oldCost += c(previous, ev[x].u);
    previous = ev[x].v;
  }
  if (nxt >= 0)
    oldCost += c(previous, ev[nxt].u);
  if (oldCost <= 0)
    return false;

  int states = 1 << length, full = states - 1;
  vi pre(length, 0);
  for (int j = 0; j < length; j++)
    if (events[j] & 1) {

      for (int i = 0; i < length; i++)
        if (events[i] == (events[j] ^ 1))
          pre[j] |= 1 << i;
    }
  vl ld(states, w0);
  for (int mask = 1; mask < states; mask++) {
    int bit = mask & -mask, index = 0;
    while ((1 << index) != bit)
      index++;
    ld[mask] = ld[mask ^ bit] + ev[events[index]].w;
  }

  struct Label {
    ll time, cst;
    int parent, event;
  };

  constexpr size_t MAXLAB = 100000;
  vc<Label> labels;
  labels.reserve(MAXLAB);
  labels.push_back({t0, 0, -1, -1});
  vc<vi> fronts(size_t(states) * length);
  ll bestCost = oldCost;
  int winning = -1;
  bool stopped = false;
  uint64_t operations = 0;

  auto extend = [&](int mask, int last, int labelId, int next) {
    if (stopped)
      return;
    if (((++operations) & 255) == 0 && out(until)) {
      stopped = true;
      return;
    }
    int bit = 1 << next;
    if ((mask & bit) || (mask & pre[next]) != pre[next])
      return;
    int nextMask = mask | bit;
    if (ld[nextMask] < 0 || ld[nextMask] > Q[k])
      return;

    Label parent = labels[labelId];
    int u = last < 0 ? u0 : ev[events[last]].v;
    const Node& event = ev[events[next]];
    ll arrival = max(event.l, parent.time + t(u, event.u));
    if (arrival > event.r)
      return;
    ll finish = arrival + event.dt;
    ll value = parent.cst + c(u, event.u);

    if (value >= bestCost)
      return;
    ll totCost = value;
    if (nextMask == full && nxt >= 0) {
      const Node& suffix = ev[nxt];
      if (finish + t(event.v, suffix.u) > original.lim[end])
        return;
      totCost += c(event.v, suffix.u);
    }
    if (nextMask == full && totCost >= bestCost)
      return;
    auto& front = fronts[size_t(nextMask) * length + next];
    for (int id : front) {
      const auto& old = labels[id];
      if (old.time <= finish && old.cst <= value)
        return;
    }
    if (labels.size() >= MAXLAB) {
      stopped = true;
      return;
    }
    front.erase(remove_if(all(front),
                          [&](int id) {
                            const auto& old = labels[id];
                            return finish <= old.time && value <= old.cst;
                          }),
                front.end());
    int newId = sz(labels);
    labels.push_back({finish, value, labelId, next});
    front.push_back(newId);
    if (nextMask == full) {
      bestCost = totCost;
      winning = newId;
    }
  };

  for (int next = 0; next < length && !stopped; next++)
    extend(0, -1, 0, next);
  for (int mask = 1; mask < full && !stopped; mask++) {
    if ((mask & 31) == 0 && out(until)) {
      stopped = true;
      break;
    }
    for (int last = 0; last < length && !stopped; last++)
      if (mask & (1 << last)) {

        const auto& front = fronts[size_t(mask) * length + last];
        for (int id : front) {
          for (int next = 0; next < length && !stopped; next++)
            if (!(mask & (1 << next)))
              extend(mask, last, id, next);
          if (stopped)
            break;
        }
      }
  }
  if (winning < 0)
    return false;
  vi order;
  for (int id = winning; labels[id].parent >= 0; id = labels[id].parent)
    order.push_back(events[labels[id].event]);
  if (sz(order) != length)
    return false;
  reverse(all(order));
  Rt candidate = original;
  copy(all(order), candidate.a.begin() + begin);
  if (!eval(k, candidate) || candidate.f <= original.f)
    return false;
  s.f += candidate.f - original.f;
  s.a[k] = move(candidate);

  save(k, s.a[k]);
  upd(s);
  return true;
}

void ls(Sol& s, double until) {
  double t0 = getTime(), span = max(0.0, until - t0);
  double te = t0 + 0.30 * span;
  for (int rep = 0; rep < min(K, 3) && !out(te); rep++)
    optRoute(s, rd(K), te);
  double tb = t0 + 0.50 * span;
  for (int rep = 0; rep < 2 && !out(tb); rep++) {
    int k = rd(K), length = sz(s.a[k].a);
    int width = min(length, tb - getTime() < 0.003 ? 8 : 11);
    if (width < 2)
      continue;
    optBlock(s, k, rd(length - width + 1), width, tb);
  }
  double te2 = t0 + 0.85 * span;
  vi ids(R);
  iota(all(ids), 0);
  shuffle(all(ids), rng);

  stable_partition(all(ids), [&](int r) { return s.id[r] >= 0; });
  int budget = min(int(used(s).size()), 32);
  for (int z = 0; z < budget && !out(te2); z++) {
    int r = ids[z], old = s.id[r];
    Sol b = s;
    if (old >= 0)
      eraseReq(b, {r});
    Sol candidate = b;
    auto p = bestMove(b, r, te2, old);
    if (p.ok()) {
      put(b, r, p);
      if (b.f > candidate.f)
        candidate = move(b);
    }
    if (candidate.f > s.f) {
      s = move(candidate);
      upd(s);
    }
  }

  for (int rep = 0; rep < 12 && !out(te2); rep++) {
    auto active = used(s);
    if (active.empty() || R < 2)
      break;
    int a = active[rd(sz(active))], b = rd(R);
    if (rep % 2 == 0) {
      vi outside;
      for (int r = 0; r < R; r++)
        if (s.id[r] < 0)
          outside.push_back(r);
      if (!outside.empty()) {
        b = outside[rd(sz(outside))];
        int candidate = outside[rd(sz(outside))];
        if (rq[candidate].rev > rq[b].rev)
          b = candidate;
        if (rnd() < 0.6) {
          double nearest = 1e300;
          for (int sample = 0; sample < min(12, sz(active)); sample++) {
            int id = active[rd(sz(active))];
            if (rq[b].w > Q[s.id[id]])
              continue;
            double related = double(t(rq[id].p, rq[b].p)) + double(t(rq[id].d, rq[b].d)) +
                             0.25 * abs(double(rq[id].e) - double(rq[b].e));
            if (related < nearest) {
              nearest = related;
              a = id;
            }
          }
        }
      }
    }
    if (a == b)
      continue;
    Sol base = s;
    eraseReq(base, {a, b});
    if (base.f > s.f) {
      s = base;
      upd(s);
    }
    for (int order = 0; order < 2 && !out(te2); order++) {
      Sol trial = base;
      int x = order ? a : b, y = order ? b : a;
      auto p = bestMove(trial, x, te2, s.id[x]);
      if (!p.ok())
        continue;
      put(trial, x, p);
      if (trial.f > s.f) {
        s = trial;
        upd(s);
      }
      auto q = bestMove(trial, y, te2, s.id[y]);
      if (q.ok() && q.gain > 0)
        put(trial, y, q);
      if (trial.f > s.f) {
        s = move(trial);
        upd(s);
      }
    }
  }
  crossTail(s, until);
}

void learn(const Sol& elite) {
  constexpr double rho = 0.12;
  for (int r = 0; r < R; r++)
    for (int k = 0; k < K; k++)
      tau(r, k) = clamp((1 - rho) * tau(r, k) + rho * (elite.id[r] == k ? 1.0 : 0.0), 0.02, 0.98);
}
