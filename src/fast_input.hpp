#pragma once

// Minimal integer reader over stdin. Accepts '#' comments, rejects malformed
// numbers and values that would overflow `ll`, so input() never sees garbage.

struct FastIn {
  static constexpr size_t SZ = 1 << 16;
  char buffer[SZ];
  size_t pos = 0, len = 0;

  int get() {
    if (pos == len) {
      len = fread(buffer, 1, SZ, stdin);
      pos = 0;
      if (!len)
        return EOF;
    }
    return static_cast<unsigned char>(buffer[pos++]);
  }

  ll next() {
    int c = get();
    for (;;) {
      while (c != EOF && c <= 32)
        c = get();
      if (c != '#')
        break;
      while (c != EOF && c != '\n')
        c = get();
    }
    if (c == EOF)
      throw runtime_error("Unexpected end of input");
    bool neg = c == '-';
    if (neg)
      c = get();
    if (c < '0' || c > '9')
      throw runtime_error("Expected an integer");
    ll x = 0;
    while (c >= '0' && c <= '9') {
      if (x > (INF - (c - '0')) / 10)
        throw runtime_error("Integer is too large");
      x = x * 10 + c - '0';
      c = get();
    }
    if (c == '#')
      while (c != EOF && c != '\n')
        c = get();
    else if (c != EOF && c > 32)
      throw runtime_error("Invalid character after integer");
    return neg ? -x : x;
  }
};
