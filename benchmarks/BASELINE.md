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

| benchmark | baseline | notes |
|---|---|---|
| BM_Solve | **2.12 ms / solve** | avg 23.2 moves (9.5 phase 1 + 13.7 phase 2) |
| BM_IdaPhase1 | 0.096 ms | |
| BM_IdaPhase2 | 2.08 ms | |

Throughput implication: ~472 solves/sec single-threaded.

## Progress — per-solve latency

| optimization | BM_Solve | speedup vs baseline | notes |
|---|---|---|---|
| baseline | 2.12 ms | 1.00x | |
| cache table refs in DFS via Ctx | 1.31 ms | **1.62x** | hoist 5 static-local getters out of the recursion |

## Progress — throughput under concurrency (BM_SolveThroughput)

Reentrant solver, no shared writable state. Numbers from 3s min-time runs
per thread count.

| threads | per-thread latency | total solves/sec | scaling vs 1 thread |
|---|---|---|---|
| 1  | 1.34 ms | 745  | 1.00x |
| 2  | 1.36 ms | 1,470 | 1.97x |
| 4  | 1.37 ms | 2,923 | 3.92x |
| 8  | 1.39 ms | 5,749 | 7.72x |
| 16 | 2.18 ms | 7,337 | 9.84x (14 cores — 16 oversubscribes) |

near-linear scaling through the physical core count. per-thread latency
barely moves as threads grow — the solver is not fighting any shared state.

## Where the time goes

Phase 2 is ~98% of total solve time (2.08 ms out of 2.12 ms). Any optimization
to phase 2 dominates the overall speedup. Phase 1 is essentially free — its
pruning table is tight and it finds short solutions (9.5 moves avg) quickly.

## Phase 5 targets (from ROADMAP)

- Multithreaded IDA* — primary lever.
- Cache-friendly bitboard / coordinate-encoded state layout — secondary.
- Report before -> after ratios per optimization, not a single blended number.
