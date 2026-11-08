#include <cstdio>
#include "common.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static Order L(uint64_t id, Side s, int64_t p, int32_t q) { return {id, p, q, s, Type::Limit}; }
static Order M(uint64_t id, Side s, int32_t q) { return {id, 0, q, s, Type::Market}; }
static Order C(uint64_t id) { return {id, 0, 0, Side::Buy, Type::Cancel}; }

static void unit() {
  Engine e; std::vector<Trade> t;
  // price-time: two asks at 100 (id1 first), better ask at 99 (id3) must fill first, then id1, then id2
  e.process(L(1, Side::Sell, 100, 5), t); e.process(L(2, Side::Sell, 100, 5), t); e.process(L(3, Side::Sell, 99, 5), t);
  CHECK(t.empty());
  int n = e.process(L(4, Side::Buy, 100, 12), t);
  CHECK(n == 3 && t[0].maker == 3 && t[0].price == 99 && t[1].maker == 1 && t[2].maker == 2 && t[2].qty == 2);
  CHECK(e.resting() == 1);                       // id2 has 3 left
  CHECK(e.process(C(2), t) == 1);                // cancel remaining
  CHECK(e.process(C(2), t) == 0);                // double cancel
  CHECK(e.process(C(1), t) == 0);                // cancel of filled order
  t.clear();
  CHECK(e.process(M(5, Side::Buy, 10), t) == 0 && e.resting() == 0);  // market on empty book: no rest
  // limit remainder rests, market never rests, non-crossing limit doesn't trade
  e.process(L(6, Side::Buy, 50, 10), t); CHECK(e.process(L(7, Side::Sell, 51, 10), t) == 0 && e.resting() == 2);
  CHECK(e.process(M(8, Side::Sell, 25), t) == 1 && t[0].price == 50 && t[0].qty == 10 && e.resting() == 1);
}

int main() {
  unit();
  size_t total_bad = 0;
  for (uint64_t seed = 1; seed <= 20; ++seed) {
    auto ops = generate(seed, 50000);
    Engine e1, e2; RefModel r;
    Log a = replay(e1, ops), b = replay(e2, ops), c = replay(r, ops);
    CHECK(hash_log(a) == hash_log(b));           // deterministic replay
    CHECK(e1.resting() == r.resting());
    size_t bad = compare(ops, a, c);
    total_bad += bad;
  }
  CHECK(total_bad == 0);
  std::printf("%s: unit + 20 seeds x 50k ops, mismatches vs reference = %zu\n", fails ? "FAILED" : "PASSED", total_bad);
  return fails ? 1 : 0;
}
