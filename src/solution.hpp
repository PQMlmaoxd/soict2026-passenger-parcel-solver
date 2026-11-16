#pragma once

// Route evaluation, plan validation, incumbent and route archive.
//
// eval() is the single feasibility and profit oracle. Every route the search
// builds or edits goes through it before it can enter a plan, and no routine
// assumes that deleting a stop keeps a route feasible: the matrices are not
// metric.

// A plan with every taxi idle and no request served.
Sol emptySol() {
  Sol s;
  s.a.resize(K);
  s.id.assign(R, -1);
  return s;
}

// Simulates route z on taxi k. Returns false if a window or the capacity is
// violated or a parcel stays on board. On success fills tm, w, lim and f.
bool eval(int k, Rt& z) {
  int L = sz(z.a), u = O[k];
  ll tm = 0, w = 0, value = 0;
  z.tm.resize(L);
  z.lim.resize(L);
  z.w.resize(L);
  for (int i = 0; i < L; i++) {
    const Node& b = ev[z.a[i]];
    tm = max(b.l, tm + t(u, b.u));
    if (tm > b.r)
      return false;
    tm += b.dt;
    w += b.w;
    if (w < 0 || w > Q[k])
      return false;
    value += b.val - b.ic - c(u, b.u);
    z.tm[i] = tm;
    z.w[i] = w;
    u = b.v;
  }
  if (w != 0)
    return false;
  for (int i = L - 1; i >= 0; i--) {
    z.lim[i] = ev[z.a[i]].r;
    if (i + 1 < L)
      z.lim[i] = min(z.lim[i], z.lim[i + 1] - ev[z.a[i]].dt - t(ev[z.a[i]].v, ev[z.a[i + 1]].u));
  }
  z.f = value;
  return true;
}

void syncSol(Sol& s) {
  s.id.assign(R, -1);
  s.f = 0;
  for (int k = 0; k < K; k++) {
    s.f += s.a[k].f;
    for (int x : s.a[k].a)
      s.id[x / 2] = k;
  }
}

// Validates a whole plan: events are paired and on one taxi, every route passes
// eval(), and the request-to-taxi map matches. Used before output.
bool check(Sol& s) {
  vi state(R, 0), assigned(R, -1);
  for (int k = 0; k < K; k++) {
    for (int x : s.a[k].a) {
      if (x < 0 || x >= 2 * R)
        return false;
      int r = x / 2;
      if (isP(r)) {
        if ((x & 1) || state[r])
          return false;
        state[r] = 2;
        assigned[r] = k;
      } else if (!(x & 1)) {
        if (state[r])
          return false;
        state[r] = 1;
        assigned[r] = k;
      } else {
        if (state[r] != 1 || assigned[r] != k)
          return false;
        state[r] = 2;
      }
    }
    if (!eval(k, s.a[k]))
      return false;
  }
  for (int x : state)
    if (x == 1)
      return false;
  syncSol(s);
  return true;
}

// Archives a positive route in pool[k], one entry per request set. When the
// per-taxi cap is exceeded it evicts the weakest route, three times out of four
// among the most crowded size class to keep the archive diverse.
void save(int k, const Rt& z) {
  if (z.f <= 0 || z.a.empty())
    return;
  vi ids;
  for (int x : z.a)
    if (!(x & 1))
      ids.push_back(x / 2);
  sort(all(ids));
  auto& columns = pool[k];
  for (auto& col : columns)
    if (col.ids == ids) {
      if (z.f > col.f)
        col = {z.a, move(ids), z.f};
      return;
    }
  columns.push_back({z.a, move(ids), z.f});
  int cap = max(16, min(160, 12000 / max(1, K)));
  if (sz(columns) > cap) {

    auto bucket = [](size_t n) {
      return n <= 2 ? 0 : n <= 4 ? 1 : n <= 8 ? 2 : 3;
    };
    array<int, 4> counts{};
    for (const auto& col : columns)
      counts[bucket(col.ids.size())]++;
    int crowded = int(max_element(all(counts)) - counts.begin());
    bool diversity = rnd() < 0.75;
    int worst = -1;
    for (int i = 0; i < sz(columns); i++) {
      if (diversity && bucket(columns[i].ids.size()) != crowded)
        continue;
      if (worst < 0 || columns[i].f < columns[worst].f)
        worst = i;
    }
    columns.erase(columns.begin() + worst);
  }
}

// Replaces the incumbent if s is better. Routes with non-positive benefit are
// emptied first, and the routes of the new incumbent go to the archive.
void upd(const Sol& s) {
  ll cleaned = s.f;
  for (const auto& route : s.a)
    if (route.f < 0)
      cleaned -= route.f;
  if (cleaned > ans.f) {
    ans = s;

    for (auto& route : ans.a)
      if (route.f <= 0)
        route = Rt();
    syncSol(ans);
    for (int k = 0; k < K; k++)
      save(k, ans.a[k]);
  }
}
