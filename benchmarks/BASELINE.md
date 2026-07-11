# Phase 5 baseline

Captured 2026-07-11 on Apple Silicon (14-core, L1D 64 KiB, L2 4 MiB per cluster).
Build: `-DCMAKE_BUILD_TYPE=Release` (default), Apple Clang, single-threaded solver.

Command:
```
cmake --build build --target cube_benchmarks
./build/benchmarks/cube_benchmarks \
    --benchmark_filter='BM_Solve|BM_IdaPhase1|BM_IdaPhase2' \
    --benchmark_min_time=5s
```

## Numbers to beat

| benchmark | time |
|---|---|
| BM_Solve | **2.12 ms / solve** — avg 23.2 moves (9.5 phase 1 + 13.7 phase 2) |
| BM_IdaPhase1 | 0.096 ms |
| BM_IdaPhase2 | 2.08 ms |

Throughput implication: ~472 solves/sec single-threaded.

## Where the time goes

Phase 2 is ~98% of total solve time (2.08 ms out of 2.12 ms). Any optimization
to phase 2 dominates the overall speedup. Phase 1 is essentially free — its
pruning table is tight and it finds short solutions (9.5 moves avg) quickly.

## Phase 5 targets (from ROADMAP)

- Multithreaded IDA* — primary lever.
- Cache-friendly bitboard / coordinate-encoded state layout — secondary.
- Report before -> after ratios per optimization, not a single blended number.
