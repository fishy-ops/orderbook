// Deterministic order generator + replay/compare helpers.
#pragma once
#include <cstdio>
#include "engine.h"
#include "reference.h"

struct Rng {  // xorshift64*, fully deterministic across platforms
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ull; }
  uint32_t below(uint32_t n) { return (uint32_t)((next() >> 33) % n); }
};

// ~55% limit, ~10% market, ~35% cancel (some target already-filled or unknown ids on purpose).
inline std::vector<Order> generate(uint64_t seed, size_t n) {
  Rng r(seed);
  std::vector<Order> ops; ops.reserve(n);
  uint64_t next_id = 1;
  for (size_t i = 0; i < n; ++i) {
    uint32_t k = r.below(100);
    Side side = r.below(2) ? Side::Buy : Side::Sell;
    if (k < 55) {
      ops.push_back({next_id++, 10000 + (int64_t)r.below(41) - 20, (int32_t)r.below(100) + 1, side, Type::Limit});
    } else if (k < 65) {
      ops.push_back({next_id++, 0, (int32_t)r.below(150) + 1, side, Type::Market});
    } else {
      uint64_t target = next_id > 1 ? 1 + r.next() % (next_id + 5) : 1;  // may exceed issued ids
      ops.push_back({target, 0, 0, side, Type::Cancel});
    }
  }
  return ops;
}

struct Log {
  std::vector<int> rets;
  std::vector<Trade> trades;
};

template <class M>
Log replay(M& m, const std::vector<Order>& ops) {
  Log l; l.rets.reserve(ops.size());
  for (const Order& o : ops) l.rets.push_back(m.process(o, l.trades));
  return l;
}

// Returns number of mismatching ops (first few printed).
inline size_t compare(const std::vector<Order>& ops, const Log& a, const Log& b) {
  size_t bad = 0, ta = 0, tb = 0;
  if (a.rets.size() != b.rets.size()) return ops.size();
  for (size_t i = 0; i < ops.size(); ++i) {
    bool ok = a.rets[i] == b.rets[i];
    for (int k = 0; ok && k < a.rets[i] && ops[i].type != Type::Cancel; ++k)
      ok = ta + k < a.trades.size() && tb + k < b.trades.size() && a.trades[ta + k] == b.trades[tb + k];
    if (ops[i].type != Type::Cancel) { ta += a.rets[i]; tb += b.rets[i]; }
    if (!ok && ++bad <= 5) std::fprintf(stderr, "mismatch at op %zu (id %llu)\n", i, (unsigned long long)ops[i].id);
  }
  if (a.trades.size() != b.trades.size()) ++bad;
  return bad;
}

inline uint64_t hash_log(const Log& l) {  // FNV-1a, for run-to-run determinism checks
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  for (int r : l.rets) mix((uint64_t)r);
  for (const Trade& t : l.trades) { mix(t.taker); mix(t.maker); mix((uint64_t)t.price); mix((uint64_t)t.qty); }
  return h;
}
