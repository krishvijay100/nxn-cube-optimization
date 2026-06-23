# Roadmap — nxn-cube-optimization

A high-performance N×N Rubik's Cube solver in C++. This document is the map: what we're building, in what order, and why.

It is updated as the project progresses. Phase-level detail for all phases; step-level detail for the current phase only.

---

## Goals

- **Primary:** A working N×N (N = 3 through 7) Rubik's Cube solver with a web visualizer, used as a portfolio/interview piece.
- **Secondary:** Learn high-performance C++ — data layout, profiling, cache behavior, multithreading, SIMD.

These goals are co-equal. When they trade off, lean toward whichever produces a stronger demonstrable artifact.

---

## Non-goals

Things this project explicitly will **not** do. Listed so we can resist scope creep:

- **Provably optimal solving.** Kociemba converges to near-optimal on 3×3, which is the chosen ceiling. No Korf-style optimal search.
- **Sub-3×3 cubes** (Pocket Cube 2×2).
- **N > 7.** Cap is the largest WCA-recognized size; engine should not actively break on larger N but is not tested or supported there.
- **Mobile, desktop, or native apps.** Web visualizer only.
- **User accounts, persistence, scramble history.** Visualizer is stateless.
- **Real-time multiplayer / racing / leaderboards.**

---

## Architecture

```
N×N scramble
   │
   ▼
[Reduction front-end]  ── parameterized by N
  ├── Solve centers
  ├── Pair edges
  └── Fix parity (even N only)
   │
   ▼
Valid 3×3 state
   │
   ▼
[Kociemba two-phase solver]  ── algorithmic heart of the project
  ├── Phase 1: IDA* search to target subgroup
  ├── Phase 2: IDA* search to solved
  └── Optional: iterate for convergence
   │
   ▼
Move sequence (internal: uint8_t list)
   │
   ▼
[Notation layer] ── translates to standard cubing notation: "R U R' U'..."
   │
   ▼
[REST API] → [three.js visualizer]
```

Two front-ends, one solver core: 3×3 hits Kociemba directly; N×N goes through reduction first.

---

## Definition of done

Two milestones:

### Vertical slice — 3×3 end-to-end (checkpoint, not ceiling)

The project is *shippable as a 3×3-only thing* at this point. Reaching this milestone guarantees a real resume artifact even if later phases stall.

- Cube engine for 3×3 with full move set
- Random scramble generation
- Kociemba two-phase solver producing solutions, ideally near 20 moves
- Performance pass with before/after benchmarks
- REST API exposing the solver
- three.js 3D visualizer that animates the solution
- Solve statistics displayed (move count, solver time)

### Final — full N×N

The full project goal.

- All of the above, generalized to N = 3 through 7
- Reduction front-end (centers, edge pairing, parity fix) integrated and tested
- Visualizer renders any supported N
- Documented and demoable

---

## Phases

Phase-level only for the full project. The current phase is expanded into steps below.

| # | Phase | Output | Status |
|---|---|---|---|
| 1 | **Project scaffolding** | CMake project, GoogleTest, Google Benchmark, hello-world build pipeline | ✅ done |
| 2 | **3×3 cube engine** | `CubeState` + move application + invariant checks + tests/benchmarks | ✅ done |
| 3 | **3×3 scramble generator** | Random valid scrambles, reproducible with a seed | 🟡 next |
| 4 | **Kociemba two-phase solver (3×3)** | IDA* search, pruning tables, near-optimal solutions in milliseconds | ⏳ |
| 5 | **Performance pass on 3×3 solver** | Profiling, bitboards, cache-friendly layout, multithreading, possibly SIMD; before/after benchmarks | ⏳ |
| 6 | **REST API + three.js visualizer (3×3)** | End-to-end browser demo. **Vertical slice complete.** | ⏳ |
| 7 | **Generalize cube engine to N×N** | Same `CubeState` shape parameterized by N; all moves work for any N | ⏳ |
| 8 | **Reduction front-end** | Center-solving, edge-pairing, parity-fix stages | ⏳ |
| 9 | **Integrate reduction + Kociemba for N×N** | Full pipeline: N×N scramble → reduced state → solved | ⏳ |
| 10 | **Generalize visualizer + polish** | Visualizer renders N=3–7; final benchmarks, docs, README, demo video | ⏳ |

---

## Current phase — Phase 3: 3×3 scramble generator

Goal: produce random valid scrambles, reproducible from a seed, with sensible move-sequence filtering (no `R R'` adjacent, no same-face repeats).

Phase 2 baseline (for reference when Phase 5 perf pass lands):
- Single `apply_move`: ~10 ns
- 18-move scramble: ~179 ns total (~100M moves/sec throughput)

---

## Working agreement

- All code in C++20, built with Clang + Ninja, default Release mode.
- All non-trivial design decisions are surfaced for discussion before code is written.
- Tests and benchmarks are written alongside features, not after.
- This document is updated when phases complete or scope shifts.
