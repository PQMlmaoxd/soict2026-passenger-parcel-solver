#include "common.hpp"
#include "fast_input.hpp"
#include "types.hpp"
#include "state.hpp"
#include "solution.hpp"
#include "insertion.hpp"
#include "alns_operators.hpp"
#include "local_search.hpp"
#include "recombination.hpp"
#include "exact_small.hpp"
#include "exact_sparse.hpp"
#include "input.hpp"
#include "sisr.hpp"
#include "solver.hpp"

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  double sec = 295;
  uint64_t seed = 42;
  bool use = true;
  try {
    for (int i = 1; i < argc; i++) {
      string arg = argv[i];
      if (arg == "--time" && i + 1 < argc)
        sec = stod(argv[++i]);
      else if (arg == "--seed" && i + 1 < argc)
        seed = stoull(argv[++i]);
      else if (arg == "--no-exact")
        use = false;
      else
        return 1;
    }
    if (!isfinite(sec) || sec < 0 || sec > 298)
      return 1;
    init(sec, seed, use);
    input();
  } catch (...) {
    return 1;
  } // malformed input / bad args: as before
  try {
    solve();
  } catch (...) {
    recoverAns();
  }
  try {
    print();
  } catch (...) {
    return 1;
  }
  cout.flush();
  fflush(stdout);
  std::_Exit(0); // skip teardown of large containers
}
