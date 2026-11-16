#pragma once

// Core data types.
//
// Events. A passenger r is one event with id 2r: pickup, direct ride and
// drop-off happen back to back. A parcel r is two events, pickup 2r and
// delivery 2r + 1. Event x therefore belongs to request x / 2, and (x & 1)
// marks a delivery. A route is a vector of event ids.
//
//   Req   one request as read from the input.
//   Node  one event: entry/exit vertex u, v, start window [l, r], duration dt,
//         load change w, revenue credited val, internal travel cost ic.
//   Rt    a route plus the data eval() derives from it (completion times tm,
//         loads w, latest feasible start lim, benefit f).
//   Sol   one route per taxi, request -> taxi map id, total benefit f.
//   Ins   an insertion move: taxi k, positions i (pickup) and j (delivery).
//   Col   an archived route (column) with its sorted request ids.
//   ALNS  adaptive weights of a group of operators (roulette-wheel selection).

struct Req {
  int p, d;
  ll w = 0, e, l, ed = 0, ld = INF, s, rev;
};

struct Node {
  int u, v;
  ll l, r, dt, w, val, ic;
};

struct Rt {
  vi a;
  vl tm, lim, w;
  ll f = 0;
};

struct Sol {
  vc<Rt> a;
  vi id;
  ll f = 0;
};

struct Ins {
  int k = -1, i = 0, j = 0;
  ll gain = BAD, delay = 0;

  bool ok() const {
    return k >= 0;
  }
};

struct Col {
  vi a, ids;
  ll f;
};

struct ALNS {
  vc<double> wgt, scr;
  vi cnt;

  explicit ALNS(int n) : wgt(n, 1), scr(n, 0), cnt(n, 0) {
  }

  int pick(mt19937_64& rng) {
    return discrete_distribution<int>(all(wgt))(rng);
  }

  void val(int x, double v) {
    scr[x] += v;
    cnt[x]++;
  }

  void update() {
    for (size_t i = 0; i < wgt.size(); i++) {
      if (cnt[i])
        wgt[i] = max(0.1, 0.8 * wgt[i] + 0.2 * scr[i] / cnt[i]);
      scr[i] = 0;
      cnt[i] = 0;
    }
  }
};
