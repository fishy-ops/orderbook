# matching-engine

Single-threaded price-time-priority matching engine (limit / market / cancel) fed by a lock-free SPSC queue.

    make check   # unit cases + 20 seeded replays, engine vs naive reference model, plus run-twice determinism
    make run     # 1M-order benchmark: p50/p99 match latency, orders/sec, mismatches vs reference

Files: `engine.h` (book), `reference.h` (naive O(n) model), `spsc_queue.h`, `common.h` (seeded generator, replay, compare), `test.cpp`, `bench.cpp`.

Notes: trades print at the resting order's price; market remainders are dropped; cancel of a filled/unknown id returns 0.
Apple Silicon's clock ticks every ~41 ns, so latencies are quantized to that step; the bench prints the timer floor.
