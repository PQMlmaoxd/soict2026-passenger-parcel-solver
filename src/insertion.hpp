#pragma once

// Insertion moves and removal of requests.
//
// findMove() scans every position where request r can enter taxi k. It uses
// the latest-start slack stored in the route (Rt::lim) to test the unchanged
// suffix in constant time per position. taxiList() chooses which taxis to try.

vi makeRoute(const vi& a, int r, const Ins& p) {
  vi b;
  b.reserve(a.size() + 2);
  for (int i = 0; i <= sz(a); i++) {
    if (i == p.i)
      b.push_back(2 * r);
    if (!isP(r) && i == p.j)
      b.push_back(2 * r + 1);
    if (i < sz(a))
      b.push_back(a[i]);
  }
  return b;
}

void put(Sol& s, int r, const Ins& p) {
  Rt z;
  z.a = makeRoute(s.a[p.k].a, r, p);
  if (!eval(p.k, z))
    throw logic_error("Invalid insertion");
  if (z.f - s.a[p.k].f != p.gain)
    throw logic_error("Stale insertion gain");
  s.f += z.f - s.a[p.k].f;
  s.a[p.k] = move(z);
  s.id[r] = p.k;
}

// Best insertion of request r into taxi k by gain (revenue minus added cost).
// The delay estimate feeds the delay-aware repair modes.
Ins findMove(const Sol& s, int r, int k, double until) {
  Ins bestMove;
  const Rt& z = s.a[k];
  const Req& q = rq[r];
  if (q.w > Q[k])
    return bestMove;
  int L = sz(z.a);
  for (int i = 0; i <= L; i++) {
    if ((i & 15) == 0 && out(until))
      break;
    int prev = i ? ev[z.a[i - 1]].v : O[k];
    ll before = i ? z.tm[i - 1] : 0;
    ll begin = max(q.e, before + t(prev, q.p));
    if (begin > q.l)
      continue;
    ll oldEdge = i < L ? c(prev, ev[z.a[i]].u) : 0;
    if (isP(r)) {
      ll finish = begin + ev[2 * r].dt;
      if (i < L && finish + t(q.d, ev[z.a[i]].u) > z.lim[i])
        continue;
      ll delta = c(prev, q.p) + c(q.p, q.d) - oldEdge;
      if (i < L)
        delta += c(q.d, ev[z.a[i]].u);
      ll gain = q.rev - delta;
      ll delay = i < L ? max<ll>(0, finish + t(q.d, ev[z.a[i]].u) - (z.tm[i] - ev[z.a[i]].dt))
                       : finish - before;
      if (gain > bestMove.gain)
        bestMove = {k, i, i, gain, delay};
      continue;
    }
    ll baseLoad = i ? z.w[i - 1] : 0;
    if (baseLoad + q.w > Q[k])
      continue;
    ll tm = begin + q.s;
    int u = q.p;
    ll pickupDelta = c(prev, q.p) - oldEdge;
    if (i < L)
      pickupDelta += c(q.p, ev[z.a[i]].u);
    for (int j = i; j <= L; j++) {
      if ((j & 63) == 0 && out(until))
        break;
      ll delivery = max(q.ed, tm + t(u, q.d));
      if (delivery <= q.ld && (j == L || delivery + q.s + t(q.d, ev[z.a[j]].u) <= z.lim[j])) {
        ll delta;
        if (j == i) {
          delta = c(prev, q.p) + c(q.p, q.d) - oldEdge;
          if (j < L)
            delta += c(q.d, ev[z.a[j]].u);
        } else {
          delta = pickupDelta + c(u, q.d);
          if (j < L)
            delta += c(q.d, ev[z.a[j]].u) - c(u, ev[z.a[j]].u);
        }
        ll gain = q.rev - delta;
        ll delay =
            j < L ? max<ll>(0, delivery + q.s + t(q.d, ev[z.a[j]].u) - (z.tm[j] - ev[z.a[j]].dt))
                  : delivery + q.s - (L ? z.tm.back() : 0);
        if (gain > bestMove.gain)
          bestMove = {k, i, j, gain, max<ll>(0, delay)};
      }
      if (j == L)
        break;
      const Node& b = ev[z.a[j]];
      tm = max(b.l, tm + t(u, b.u));
      if (tm > b.r || z.w[j] + q.w > Q[k])
        break;
      tm += b.dt;
      u = b.v;
    }
  }
  return bestMove;
}

// Taxis worth trying for request r: all of them when K <= 24. Otherwise the 16
// best by proximity and affinity (every ranked taxi when R <= 120 and on every
// 19th ALNS iteration), the 6 earliest to arrive, 4 random ones and `forced`.
vi taxiList(const Sol& s, int r, int forced = -1) {
  vi taxis;
  if (K <= 24) {
    taxis.resize(K);
    iota(all(taxis), 0);
    return taxis;
  }
  vc<pair<double, int>> rank, early;
  for (int k = 0; k < K; k++)
    if (rq[r].w <= Q[k]) {
      ll near = c(O[k], rq[r].p), arrival = t(O[k], rq[r].p);
      const auto& a = s.a[k].a;
      for (int i = 0; i < sz(a); i++) {
        int u = ev[a[i]].v;
        near = min(near, c(u, rq[r].p));
        arrival = min(arrival, s.a[k].tm[i] + t(u, rq[r].p));
      }
      if (max(arrival, rq[r].e) > rq[r].l)
        continue;
      double scr = double(near) - 0.1 * gs * tau(r, k) + rnd() * 0.02 * gs;
      rank.emplace_back(scr, k);
      early.emplace_back(double(arrival), k);
    }
  int cnt = min((R <= 120 || (iter > 0 && iter % 19 == 0)) ? K : 16, sz(rank));
  sort(all(rank));
  for (int i = 0; i < cnt; i++)
    taxis.push_back(rank[i].second);
  sort(all(early));
  for (int i = 0; i < min(6, sz(early)); i++)
    taxis.push_back(early[i].second);
  for (int j = 0; j < 4; j++)
    taxis.push_back(rd(K));
  if (forced >= 0)
    taxis.insert(taxis.begin(), forced);
  vc<char> seen(K, 0);
  vi unique;
  for (int k : taxis)
    if (!seen[k]) {
      seen[k] = 1;
      unique.push_back(k);
    }
  taxis = move(unique);
  return taxis;
}

Ins bestMove(const Sol& s, int r, double until, int force = -1) {
  Ins p;
  for (int k : taxiList(s, r, force)) {
    if (out(until))
      break;
    auto v = findMove(s, r, k, until);
    if (v.gain > p.gain)
      p = v;
  }
  return p;
}

vi used(const Sol& s) {
  vi ids;
  for (int r = 0; r < R; r++)
    if (s.id[r] >= 0)
      ids.push_back(r);
  return ids;
}

// Removes the given requests. A shortened route can break a time window on
// asymmetric input, so further requests are dropped until the route is feasible.
void eraseReq(Sol& s, const vi& ids) {
  vc<char> cut(R, 0), changed(K, 0);
  for (int r : ids)
    if (s.id[r] >= 0) {
      cut[r] = 1;
      changed[s.id[r]] = 1;
    }
  for (int k = 0; k < K; k++)
    if (changed[k]) {
      Rt z;
      for (int x : s.a[k].a)
        if (!cut[x / 2])
          z.a.push_back(x);
      while (!eval(k, z)) {
        int u = O[k], bad = -1;
        ll tm = 0, load = 0;
        for (int x : z.a) {
          const auto& b = ev[x];
          tm = max(b.l, tm + t(u, b.u));
          load += b.w;
          if (tm > b.r || load < 0 || load > Q[k]) {
            bad = x / 2;
            break;
          }
          tm += b.dt;
          u = b.v;
        }
        if (bad < 0) {
          z = Rt();
          eval(k, z);
          break;
        }
        z.a.erase(remove_if(all(z.a), [&](int x) { return x / 2 == bad; }), z.a.end());
      }
      s.a[k] = move(z);
    }
  syncSol(s);
}
