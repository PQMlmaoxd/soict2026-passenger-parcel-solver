#pragma once

// Shared aliases and constants, included first by every other module.
//
// The solver is one translation unit: main.cpp includes the modules in a fixed
// order and the contest build is that same text in a single file (see
// tools/amalgamate.py). Order matters, a module may only use what the modules
// above it declare.

#include <bits/stdc++.h>
using namespace std;
using ll = long long;
using vi = vector<int>;
using vl = vector<ll>;
template <class T> using vc = vector<T>;
using clk = chrono::steady_clock;
#define all(x) (x).begin(), (x).end()
#define sz(x) int((x).size())
const ll INF = LLONG_MAX / 4, BAD = -INF;
