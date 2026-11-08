// Lock-free single-producer/single-consumer ring buffer (power-of-two capacity).
#pragma once
#include <atomic>
#include <cstddef>
#include <memory>

template <class T, size_t N>
class SpscQueue {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");

 public:
  bool push(const T& v) {
    size_t t = tail_.load(std::memory_order_relaxed);
    if (t - head_cache_ == N) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (t - head_cache_ == N) return false;  // full
    }
    buf_[t & (N - 1)] = v;
    tail_.store(t + 1, std::memory_order_release);
    return true;
  }
  bool pop(T& v) {
    size_t h = head_.load(std::memory_order_relaxed);
    if (h == tail_cache_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (h == tail_cache_) return false;  // empty
    }
    v = buf_[h & (N - 1)];
    head_.store(h + 1, std::memory_order_release);
    return true;
  }

 private:
  alignas(64) std::atomic<size_t> head_{0};  // consumer-owned
  size_t tail_cache_ = 0;                    // consumer's cached tail
  alignas(64) std::atomic<size_t> tail_{0};  // producer-owned
  size_t head_cache_ = 0;                    // producer's cached head
  alignas(64) T buf_[N];
};
