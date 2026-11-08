#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "common.h"
#include "spsc_queue.h"

using Clock = std::chrono::steady_clock;
static inline uint64_t now_ns() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
  size_t N = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1000000;
  uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 42;
  auto ops = generate(seed, N);

  // timer floor: smallest nonzero step + cost of back-to-back reads
  uint64_t step = ~0ull, ov = 0;
  for (int i = 0; i < 100000; ++i) { uint64_t a = now_ns(), b = now_ns(); if (b > a) step = std::min(step, b - a); ov += b - a; }

  static SpscQueue<Order, 1 << 16> q;
  std::vector<uint32_t> lat(N);
  Log log; log.rets.reserve(N); log.trades.reserve(N);
  Engine eng; std::vector<Trade> tmp; tmp.reserve(256);

  uint64_t t0 = 0, t1 = 0;
  std::thread consumer([&] {
    Order o;
    for (size_t i = 0; i < N; ++i) {
      while (!q.pop(o)) {}
      if (i == 0) t0 = now_ns();
      tmp.clear();
      uint64_t a = now_ns();
      int r = eng.process(o, tmp);
      uint64_t b = now_ns();
      lat[i] = (uint32_t)(b - a);
      log.rets.push_back(r);
      log.trades.insert(log.trades.end(), tmp.begin(), tmp.end());
    }
    t1 = now_ns();
  });
  for (const Order& o : ops) while (!q.push(o)) {}
  consumer.join();

  std::vector<uint32_t> s = lat; std::sort(s.begin(), s.end());
  auto pct = [&](double p) { return s[std::min(N - 1, (size_t)(p * N))]; };
  double secs = (t1 - t0) / 1e9;

  RefModel ref; Log rl = replay(ref, ops);
  size_t bad = compare(ops, log, rl);

  std::printf("ops             : %zu (seed %llu), trades %zu, resting at end %zu\n", N, (unsigned long long)seed, log.trades.size(), eng.resting());
  std::printf("timer           : step %llu ns, back-to-back read ~%.0f ns (latencies are quantized to the step)\n",
              (unsigned long long)step, (double)ov / 100000);
  std::printf("match latency   : p50 %u ns, p99 %u ns, p99.9 %u ns, max %u ns\n", pct(0.50), pct(0.99), pct(0.999), s.back());
  std::printf("throughput      : %.2f M orders/sec (end-to-end via SPSC queue)\n", N / secs / 1e6);
  std::printf("mismatches      : %zu vs reference model\n", bad);
  return bad ? 1 : 0;
}
