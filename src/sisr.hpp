#pragma once

// String-removal phase (SISR) with route recombination and reassignment.
//
// After the ALNS phase the solver anneals with the ruin-and-recreate scheme of
// Christiaens and Vanden Berghe (2020), adapted to optional requests, parcels
// and nonmetric matrices. Two extra steps run inside the loop:
//
//   every 10 s   recombine the archived routes (pack or globalPack)
//   every 2.5 s  sisrAssignRoutes(): exact reassignment of whole routes to taxis
//
// Tuning knobs are read from SISR_* environment variables. The defaults are
// the values used in the contest submission, which ran without any of them set.

// ---- SISR phase ----
static double envd(const char* n, double d) {
  const char* e = getenv(n);
  return e && *e ? atof(e) : d;
}

// findMove without time checks/delay bookkeeping; each improving candidate position is skipped with
// prob thr/2^53 (blink).
static bool sisrMono = false;

struct CE {
  ll g[4];
  uint16_t i[4], j[4];
  int n = 0;

  void add(int a, int b, ll x) {
    if (n < 4) {
      g[n] = x;
      i[n] = uint16_t(a);
      j[n] = uint16_t(b);
    }
    n++;
  }
};

static Ins sisrFind(const Sol& s, int r, int k, uint64_t thr, CE* rec = nullptr) {
  Ins best;
  const Rt& z = s.a[k];
  const Req& q = rq[r];
  if (q.w > Q[k])
    return best;
  int L = sz(z.a);
  auto blink = [&]() {
    return (rng() >> 11) < thr;
  };
  for (int i = 0; i <= L; i++) {
    int prev = i ? ev[z.a[i - 1]].v : O[k];
    ll before = i ? z.tm[i - 1] : 0;
    if (i && sisrMono && before > q.l)
      break; // (4) tm[] non-decreasing when all t,dt>=0: later positions all fail begin>q.l
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
      if (rec)
        rec->add(i, i, gain);
      if (gain > best.gain && !blink())
        best = {k, i, i, gain, 0};
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
        if (rec)
          rec->add(i, j, gain);
        if (gain > best.gain && !blink())
          best = {k, i, j, gain, 0};
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
  if (rec && rec->n > 4)
    rec->n = -1;
  return best;
}

// Exact assignment of the existing paths to taxis. With a fixed event
// order only the approach to the first event changes. The backward
// latest-start label and peak load characterize that taxi's feasibility.
static bool sisrAssignRoutes(Sol& s, double until) {
  if (K < 2 || out(until))
    return false;
  vl cost(size_t(K) * K);
  for (int i = 0; i < K; i++) {
    if (out(until))
      return false;
    const auto& z = s.a[i];
    if (z.a.empty())
      continue;
    int from = ev[z.a.front()].u;
    ll peak = *max_element(all(z.w));
    for (int k = 0; k < K; k++) {
      if (c(O[k], from) > ll(1e15))
        return false; // overflow guard: keep Hungarian u/v far below INF
      cost[size_t(i) * K + k] =
          (peak <= Q[k] && t(O[k], from) <= z.lim[0]) ? c(O[k], from) : INF / 8;
    }
  }
  // Hungarian algorithm with deadline checks before committing anything.
  vl u(K + 1), v(K + 1), slack(K + 1);
  vi colToRow(K + 1), way(K + 1);
  vc<char> usedCol(K + 1);
  for (int i = 1; i <= K; i++) {
    if (out(until))
      return false;
    fill(all(slack), INF);
    fill(all(usedCol), 0);
    colToRow[0] = i;
    int col = 0;
    do {
      if (out(until))
        return false;
      usedCol[col] = 1;
      int row = colToRow[col], next = 0;
      ll delta = INF;
      for (int j = 1; j <= K; j++)
        if (!usedCol[j]) {
          ll w = cost[size_t(row - 1) * K + j - 1] - u[row] - v[j];
          if (w < slack[j]) {
            slack[j] = w;
            way[j] = col;
          }
          if (slack[j] < delta) {
            delta = slack[j];
            next = j;
          }
        }
      if (!next)
        return false;
      for (int j = 0; j <= K; j++) {
        if (usedCol[j]) {
          u[colToRow[j]] += delta;
          v[j] -= delta;
        } else
          slack[j] -= delta;
      }
      col = next;
    } while (colToRow[col]);
    do {
      int prev = way[col];
      colToRow[col] = colToRow[prev];
      col = prev;
    } while (col);
  }
  Sol candidate = emptySol();
  for (int k = 0; k < K; k++) {
    int i = colToRow[k + 1] - 1;
    if (i < 0 || cost[size_t(i) * K + k] >= INF / 8 || out(until))
      return false;
    candidate.a[k].a = s.a[i].a;
    if (!eval(k, candidate.a[k]))
      return false;
  }
  syncSol(candidate);
  if (candidate.f <= s.f)
    return false;
  s = move(candidate);
  return true;
}

static void sisrPhase(double Tsw) {
  if (!R || !K || Tsw >= TL - 0.05)
    return;
  const double cbar = envd("SISR_CBAR", 10), Lmax = envd("SISR_LMAX", 10),
               beta = envd("SISR_BETA", 0.01);
  const int UNS = int(envd("SISR_UNS", 5));
  const double swapRate = envd("SISR_SWAP", 0.25);
  const uint64_t thr = uint64_t(envd("SISR_BLINK", 0.01) * 9007199254740992.0);
  const double T0 = envd("SISR_T0", 0.3) * gs, Tf = envd("SISR_TF", 0.01) * gs;
  const uint64_t maxit = uint64_t(envd("SISR_MAXIT", 0));
  const auto rt0 = clk::now();
  const double t0 = getTime();
  if (TL - t0 < 0.2)
    return;
  cerr << "event=sisr_cfg gs=" << gs << " T0=" << T0 << " Tf=" << Tf << " cbar=" << cbar
       << " Lmax=" << Lmax << " uns=" << UNS << " start=" << t0 << "\n";
  auto dist = [&](int a, int b) -> ll {
    const auto& x = rq[a];
    const auto& y = rq[b];
    return min(t(x.p, y.p) + t(x.d, y.d), t(y.p, x.p) + t(y.d, x.d));
  };
  vc<vi> nb(R);
  for (int a = 0; a < R; a++) {
    vc<pair<ll, int>> v;
    v.reserve(R);
    for (int b = 0; b < R; b++)
      v.emplace_back(a == b ? -1 : dist(a, b), b);
    sort(all(v));
    nb[a].reserve(R);
    for (auto& x : v)
      nb[a].push_back(x.second);
  }
  vi sig(K);
  {
    map<pair<int, ll>, int> m;
    for (int k = 0; k < K; k++) {
      auto key = make_pair(O[k], Q[k]);
      auto it = m.find(key);
      if (it == m.end())
        it = m.emplace(key, sz(m)).first;
      sig[k] = it->second;
    }
  }
  vi sigSeen(K + 1, 0), routeMark(K, 0), reqMark(R, 0);
  int sg = 0, rm = 0, qm = 0;
  Sol cur = ans;
  Sol trial = cur;
  ll startBest = ans.f;
  sisrMono = true;
  for (ll x : tmx)
    if (x < 0)
      sisrMono = false;
  for (const Node& b : ev)
    if (b.dt < 0)
      sisrMono = false;

  // 2-way cache per (r,k): header {content ids, #feasible positions (-1 = >4, uncached)} kept apart
  // from the payload so hits touch little memory
  struct CH {
    uint64_t st[2] = {0, 0};
    int8_t n[2] = {0, 0};
  };

  vc<CH> cacheH(size_t(R) * K);
  vc<CE> cacheP(size_t(R) * K * 2);
  // route versions are content ids (exact: interned by full route content), so a route that returns
  // to a previous content hits the cache again
  vl vcur(K), vtri(K);
  uint64_t stampCtr = 0;
  unordered_map<string, uint64_t> intern;
  size_t internBytes = 0;
  auto internRoute = [&](const Rt& z) -> uint64_t {
    string key(reinterpret_cast<const char*>(z.a.data()), z.a.size() * sizeof(int));
    auto it = intern.find(key);
    if (it != intern.end())
      return it->second;
    internBytes += key.size() + 64;
    intern.emplace(move(key), ++stampCtr);
    return stampCtr;
  };
  for (int k = 0; k < K; k++)
    vcur[k] = vtri[k] = internRoute(cur.a[k]);
  vi tch;
  vc<char> tchF(K, 0), chgF(K, 0);
  vi cutM(R, 0);
  int cutC = 0;
  Rt zs;
  auto touch = [&](int k) {
    if (!tchF[k]) {
      tchF[k] = 1;
      tch.push_back(k);
    }
    vtri[k] = 0;
  }; // 0 = content id not yet resolved
  // same result as eraseReq(trial,ids) (ids/f/routes), O(touched) instead of O(R+K) allocs; gone =
  // requests that left the routes, unsorted
  auto eraseT = [&](const vi& ids, vi* gone) {
    ++cutC;
    vi chg;
    for (int r : ids)
      if (trial.id[r] >= 0) {
        cutM[r] = cutC;
        int k = trial.id[r];
        if (!chgF[k]) {
          chgF[k] = 1;
          chg.push_back(k);
        }
      }
    vi old;
    for (int k : chg) {
      chgF[k] = 0;
      zs.a.clear();
      for (int x : trial.a[k].a)
        if (cutM[x / 2] != cutC)
          zs.a.push_back(x);
      while (!eval(k, zs)) {
        int u = O[k], bad = -1;
        ll tm = 0, load = 0;
        for (int x : zs.a) {
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
          zs = Rt();
          eval(k, zs);
          break;
        }
        zs.a.erase(remove_if(all(zs.a), [&](int x) { return x / 2 == bad; }), zs.a.end());
      }
      for (int x : trial.a[k].a)
        if (!(x & 1)) {
          old.push_back(x / 2);
          trial.id[x / 2] = -1;
        }
      swap(trial.a[k], zs);
      for (int x : trial.a[k].a)
        if (!(x & 1))
          trial.id[x / 2] = k;
      touch(k);
    }
    trial.f = 0;
    for (int k = 0; k < K; k++)
      trial.f += trial.a[k].f;
    if (gone)
      for (int r : old)
        if (trial.id[r] < 0)
          gone->push_back(r);
  };
  auto findC = [&](int r, int k) -> Ins {
    const size_t ix = size_t(r) * K + k;
    CH& e = cacheH[ix];
    uint64_t sv = vtri[k];
    if (!sv)
      sv = vtri[k] = internRoute(trial.a[k]);
    const int w = e.st[0] == sv ? 0 : e.st[1] == sv ? 1 : -1;
    if (w >= 0) {
      const int n = e.n[w];
      if (n < 0)
        return sisrFind(trial, r, k, thr);
      Ins best;
      if (n) {
        const CE& p = cacheP[ix * 2 + w];
        for (int x = 0; x < n; x++)
          if (p.g[x] > best.gain && !((rng() >> 11) < thr))
            best = {k, p.i[x], p.j[x], p.g[x], 0};
      }
      return best;
    }
    const int vs = e.st[0] == vcur[k] ? 1
                   : e.st[1] == vcur[k]
                       ? 0
                       : (e.st[0] < e.st[1] ? 0 : 1); // never evict the accepted route's entry
    CE tmp{};
    Ins res = sisrFind(trial, r, k, thr, &tmp);
    e.st[vs] = sv;
    e.n[vs] = int8_t(tmp.n);
    if (tmp.n > 0)
      cacheP[ix * 2 + vs] = tmp;
    return res;
  };
  // same result as put(trial,r,p) without per-call vector allocations
  Rt zp;
  auto putT = [&](int r, const Ins& p) {
    zp.a.clear();
    const vi& a = trial.a[p.k].a;
    for (int i = 0; i <= sz(a); i++) {
      if (i == p.i)
        zp.a.push_back(2 * r);
      if (!isP(r) && i == p.j)
        zp.a.push_back(2 * r + 1);
      if (i < sz(a))
        zp.a.push_back(a[i]);
    }
    if (!eval(p.k, zp))
      throw logic_error("Invalid insertion");
    if (zp.f - trial.a[p.k].f != p.gain)
      throw logic_error("Stale insertion gain");
    trial.f += zp.f - trial.a[p.k].f;
    swap(trial.a[p.k], zp);
    trial.id[r] = p.k;
    touch(p.k);
  };
  uint64_t its = 0, acc = 0, imp = 0, pkImp = 0;
  // SISR_PACK>0: archive accepted routes and recombine them every SISR_PACK s with Fleet's
  // globalPack/pack (budget SISR_PACKT s)
  const double packEvery = envd("SISR_PACK", 10), packT = envd("SISR_PACKT", 1.0);
  const uint64_t psave = max<uint64_t>(1, uint64_t(envd("SISR_PSAVE", 8)));
  double nextPk = t0 + packEvery;
  const double patience = envd("SISR_PATIENCE", 1e9);
  double lastImp = t0;
  int impLog = 0;
  const char* why = "deadline";
  double nextAssign = t0 + 1.0;
  int assignmentWins = 0;
  ll assignmentGain = 0;
  while (!out(TL) && !(maxit && its >= maxit)) {
    double now = getTime();
    if (!maxit && now >= nextAssign && TL - now > 0.06) {
      nextAssign = now + 2.5;
      Sol candidate = ans;
      if (sisrAssignRoutes(candidate, min(TL - 0.025, now + 0.035))) {
        ++assignmentWins;
        assignmentGain += candidate.f - ans.f;
        upd(candidate);
        lastImp = getTime();
        cur = move(candidate);
        trial = cur;
        for (int k = 0; k < K; k++)
          vcur[k] = vtri[k] = internRoute(cur.a[k]);
      }
    }
    if (now - lastImp > patience) {
      why = "patience";
      break;
    } // time since max(sisrStart,lastImprovement)
    double prog = maxit ? double(its) / double(maxit) : min(1.0, (now - t0) / max(1e-9, TL - t0));
    double T = T0 * pow(Tf / T0, prog);
    its++;
    if (packEvery > 0 && now >= nextPk) {
      const ll before = ans.f;
      // SISR_PKMODE: 0 = adopt pack result if > cur (resets to ans), 1 = adopt only on a strict
      // pack improvement, 2 = control: no pack, reset cur to ans
      const int pkMode = int(envd("SISR_PKMODE", 1));
      Sol p = pkMode == 2                       ? ans
              : (R > 120 && R <= 512 && K >= 8) ? globalPack(min(TL, now + packT))
                                                : pack(min(TL, now + packT));
      if (ans.f > before) {
        pkImp++;
        imp++;
        lastImp = getTime();
        if (impLog++ < 2000)
          cerr << "event=sisr_pack_imp t=" << getTime() << " best=" << ans.f << "\n";
      }
      if (p.f > cur.f && (pkMode != 1 || ans.f > before)) {
        cur = move(p);
        trial = cur;
        for (int k = 0; k < K; k++)
          vcur[k] = vtri[k] = internRoute(cur.a[k]);
      }
      nextPk = getTime() + packEvery;
      if (out(TL))
        break;
    }
    if (intern.size() > 200000 || internBytes > (size_t(8) << 20)) {
      intern.clear();
      internBytes = 0;
      for (int k = 0; k < K; k++)
        vcur[k] = vtri[k] = internRoute(cur.a[k]);
    } // bound memory; ids are never reused

    vi removed, cand;
    bool swapRound = swapRate > 0 && rnd() < swapRate;
    if (swapRound) {
      int incoming = -1;
      for (int tries = 0; tries < 32; tries++) {
        int r = rd(R);
        if (cur.id[r] < 0) {
          incoming = r;
          break;
        }
      }
      if (incoming >= 0) {
        Ins winner;
        int victim = -2;
        ll bestF = BAD;
        for (int k = 0; k < K; k++) {
          Ins p = sisrFind(cur, incoming, k, 0);
          if (p.ok() && p.gain > 0 && cur.f + p.gain > bestF) {
            bestF = cur.f + p.gain;
            winner = p;
            victim = -1;
          }
        }
        int tried = 0;
        const int active = sz(used(cur));
        for (int v : nb[incoming]) {
          if (cur.id[v] < 0)
            continue;
          if (tried++ >= 12 || out(TL))
            break;
          int k = cur.id[v];
          Sol probe = cur;
          eraseReq(probe, vi{v});
          // Keep this an atomic one-for-one exchange, even on nonmetric inputs.
          if (sz(used(probe)) != active - 1)
            continue;
          Ins p = sisrFind(probe, incoming, k, 0);
          if (p.ok() && p.gain > 0 && probe.f + p.gain > bestF) {
            bestF = probe.f + p.gain;
            winner = p;
            victim = v;
          }
        }
        if (victim != -2) {
          if (victim >= 0)
            eraseT(vi{victim}, &removed);
          putT(incoming, winner);
          cand.push_back(incoming);
        }
      }
    } else {
      vi sv = used(trial);
      int seed = sv.empty() ? rd(R) : sv[rd(sz(sv))];

      if (!sv.empty()) {
        int usedRoutes = 0;
        for (int k = 0; k < K; k++)
          usedRoutes += !trial.a[k].a.empty();
        double lsmax = min(Lmax, double(sz(sv)) / max(1, usedRoutes));
        double ksmax = 4 * cbar / (1 + lsmax) - 1;
        int ks = max(1, int(1 + rnd() * max(0.0, ksmax)));
        ++rm;
        vi cut;
        int ruined = 0;
        for (int cnd : nb[seed]) {
          if (ruined >= ks)
            break;
          int k = trial.id[cnd];
          if (k < 0 || routeMark[k] == rm)
            continue;
          routeMark[k] = rm;
          ruined++;
          vi seq;
          int pos = -1;
          for (int x : trial.a[k].a)
            if (!(x & 1)) {
              if (x / 2 == cnd)
                pos = sz(seq);
              seq.push_back(x / 2);
            }
          int n = sz(seq);
          int l = max(1, min(n, int(1 + rnd() * min(lsmax, double(n)))));
          if (l < n &&
              rnd() <
                  0.5) { // split string: remove l of a window of l+m, keeping m consecutive inside
            int m = 1;
            while (l + m < n && rnd() > beta)
              m++;
            int w = l + m, lo = max(0, pos - w + 1), hi = min(pos, n - w),
                start = lo + rd(hi - lo + 1), ps = rd(l + 1);
            for (int i = 0; i < w; i++)
              if (i < ps || i >= ps + m)
                cut.push_back(seq[start + i]);
          } else {
            int lo = max(0, pos - l + 1), hi = min(pos, n - l), start = lo + rd(hi - lo + 1);
            for (int i = 0; i < l; i++)
              cut.push_back(seq[start + i]);
          }
        }
        eraseT(cut, &removed);
        sort(all(removed));
        vi neg; // routes left with negative profit are dropped entirely
        for (int k = 0; k < K; k++)
          if (trial.a[k].f < 0)
            for (int x : trial.a[k].a)
              if (!(x & 1))
                neg.push_back(x / 2);
        if (!neg.empty()) {
          eraseT(neg, nullptr);
          for (int r : neg)
            removed.push_back(r);
        }
      }
      cand = removed;
      ++qm;
      for (int r : cand)
        reqMark[r] = qm;
      int extra = 0;
      for (int cnd : nb[seed]) {
        if (extra >= UNS)
          break;
        if (trial.id[cnd] < 0 && reqMark[cnd] != qm) {
          cand.push_back(cnd);
          reqMark[cnd] = qm;
          extra++;
        }
      }
      double u = rnd() * 10;
      if (u < 4)
        shuffle(all(cand), rng);
      else if (u < 8)
        sort(all(cand), [&](int a, int b) { return rq[a].rev > rq[b].rev; });
      else
        sort(all(cand), [&](int a, int b) { return dist(seed, a) < dist(seed, b); });
      for (int r : cand) {
        Ins best;
        ++sg;
        for (int k = 0; k < K; k++) {
          if (rq[r].w > Q[k])
            continue;
          if (trial.a[k].a.empty()) {
            if (sigSeen[sig[k]] == sg)
              continue;
            sigSeen[sig[k]] = sg;
          }
          Ins v = findC(r, k);
          if (v.gain > best.gain)
            best = v;
        }
        if (best.ok() && best.gain > 0)
          putT(r, best);
      }
    }
    ll delta = trial.f - cur.f;
    bool accept = delta >= 0 || rnd() < exp(max(-700.0, double(delta) / T));
    if (trial.f > ans.f) {
      ll b = ans.f;
      upd(trial);
      if (ans.f > b) {
        imp++;
        lastImp = now;
        if (impLog++ < 2000)
          cerr << "event=sisr_imp t=" << now << " best=" << ans.f << "\n";
      }
    }
    if (accept) {
      for (int k : tch)
        for (int x : cur.a[k].a)
          if (!(x & 1))
            cur.id[x / 2] = -1;
      for (int k : tch) {
        if (!vtri[k])
          vtri[k] = internRoute(trial.a[k]);
        cur.a[k] = trial.a[k];
        vcur[k] = vtri[k];
        for (int x : cur.a[k].a)
          if (!(x & 1))
            cur.id[x / 2] = k;
      }
      cur.f = trial.f;
      acc++;
      if (packEvery > 0 && acc % psave == 0)
        for (int k : tch)
          save(k, cur.a[k]);
    } else {
      for (int k : tch)
        for (int x : trial.a[k].a)
          if (!(x & 1))
            trial.id[x / 2] = -1;
      for (int k : tch) {
        trial.a[k] = cur.a[k];
        vtri[k] = vcur[k];
        for (int x : trial.a[k].a)
          if (!(x & 1))
            trial.id[x / 2] = k;
      }
      trial.f = cur.f;
    }
    for (int k : tch)
      tchF[k] = 0;
    tch.clear();
  }
  if (maxit && its >= maxit && why[0] == 'd')
    why = "maxit";
  cerr << "event=assignment wins=" << assignmentWins << " gain=" << assignmentGain << "\n";
  const double te = getTime();
  cerr << "event=sisr_exit reason=" << why << " t=" << te << "\n";
  cerr << "event=sisr iters=" << its << " accepted=" << acc << " improvements=" << imp
       << " pack_imp=" << pkImp << " start_best=" << startBest << " end_best=" << ans.f
       << " secs=" << te - t0 << " real_secs=" << chrono::duration<double>(clk::now() - rt0).count()
       << "\n";
}
