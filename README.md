# NxN Cube Solver Engine

High-performance C++ Rubik’s Cube solver supporting 3×3 through 7×7 cubes, with a React/Three.js simulator and a FastAPI service. The engine combines a coordinate-space Kociemba two-phase IDA* search with a deterministic, orbit-aware general-N reduction pipeline.

## Highlights

- Solves 3×3–7×7 cubes through one end-to-end API.
- Routes direct 3×3 inputs and reduced NxN states through the same production Kociemba solver.
- Provides deterministic C++ tests, Python binding tests, service endpoint tests, benchmarks, and replay-based NxN verification.
- Includes a browser simulator with cube-size selection, generated or user-entered scrambles, animated solutions, and Virtual Cube Net-compatible notation.

## Architecture

```text
├── src/
│   ├── cube/                       # Compact 3×3 state, notation, moves, and scrambles
│   ├── solver/                     # Coordinates, tables, symmetry, and two-phase IDA*
│   └── cube_nxn/                   # Dynamic NxN model and general-N reduction
│       └── legacy/                 # Preserved specialized 4×4 reduction artifact
├── bindings/
│   ├── nxn_cube_py.cpp             # pybind11 interface to the C++ engine
│   └── test_bindings.py            # Native binding integration tests
├── service/
│   ├── app.py                      # FastAPI endpoints and scramble validation
│   └── tests/                      # HTTP API tests
├── web/
│   └── src/                        # React controls and Three.js NxN renderer
├── tests/                          # Native unit and end-to-end solver tests
├── benchmarks/                     # Google Benchmark targets and recorded results
├── CMakeLists.txt
└── Dockerfile
```

The native engine is divided into two solve paths. A 3×3 is encoded directly into phase-specific coordinates and searched by the Kociemba solver. A 4×4–7×7 first passes through the general-N reducer, which resolves centers, pairs edges, normalizes parity, and projects the result into the same 3×3 representation.

The pybind11 module exposes this engine to Python without duplicating solver logic. FastAPI provides the asynchronous HTTP boundary used by the React client, while the Three.js renderer maintains its own visual NxN model and animates the exact move sequence returned by the engine.

For odd-sized cubes, the accepted scramble path preserves the fixed center frame. The service and UI reject moves that cross the central slice; generated scrambles follow the supported WCA-style move model.

## 3×3 search optimizations

The 3×3 solver uses Kociemba’s two-phase IDA* algorithm over compact coordinates rather than searching full sticker states. Phase 1 searches until corner orientation, edge orientation, and the UD-slice coordinate enter the restricted subgroup. Phase 2 then solves the remaining corner, edge, and slice permutations.

Coordinate move tables replace repeated cube simulation with array lookups, while byte-wide admissible pruning tables provide lower bounds for IDA*. Each phase’s active pair of heuristic tables occupies roughly 2 MiB, fitting within the benchmark machine’s 4 MiB L2 cache. Redundant same-face and parallel-face continuations are suppressed before recursion. Together, these techniques reduce the measured search from an estimated unpruned tree of roughly 5×10¹¹ nodes to about 278,000 visited nodes per solve—a search-space collapse of approximately 1.8×10⁶×.

The recursive hot path applies several additional optimizations:

- **Cached table references:** precomputed tables are stored in a per-search context instead of resolving static accessors inside the DFS, reducing serial latency from 2.12 ms to 1.31 ms.
- **Aggressive heuristic root-branch ordering:** root moves are scored by their child heuristic and sorted so the strongest branches are expanded first at every IDA* bound.
- **Bitboard-packed Phase 2 representation:** corner, edge, and slice permutation coordinates are packed into one 64-bit value for a smaller, register-friendly DFS state.
- **Lock-free parallel fan-out:** Phase 2 root branches are statically distributed across workers without a shared work queue; relaxed atomics cancel remaining searches after a worker succeeds and combine next-bound suggestions.

All writable search state remains invocation-local, allowing concurrent solves without shared-state contention.

| Metric | Result |
|---|---:|
| Original serial baseline | 2.12 ms per solve |
| 8-thread intra-solve search | **0.575 ms per solve** |
| End-to-end latency improvement | **3.69×** |
| Average solution length | **23.2 moves** |
| Measured search-space reduction | **~1.8×10⁶×** |
| Throughput with 8 concurrent solvers | **5,749 solves/sec** |

See [`benchmarks/BASELINE.md`](benchmarks/BASELINE.md) for benchmark methodology, phase breakdowns, thread sweeps, and the complete optimization history.

## NxN reduction optimizations

The general-N solver avoids searching the complete 4×4–7×7 configuration space. Instead, it reduces the cube through a sequence of bounded permutation problems and hands the resulting 3×3-equivalent state to the optimized Kociemba solver.

Center and edge positions are classified into geometric orbits. Each orbit contains pieces that can reach one another while preserving their distance from the face boundary, so the reducer can solve these smaller permutations independently. For each active orbit, it builds a graph describing the effect of legal setup moves and discovers how a verified commutator acts on the orbit’s slots.

The target permutation is factored into 3-cycles. Each requested cycle is converted into a concrete `setup + algorithm + inverse setup` sequence: the setup maps the desired pieces onto the commutator’s native slots, the commutator cycles them, and reversing the setup restores the surrounding state. Separate models handle center classes, each wing depth, and the odd-N middle edge. OLL and PLL parity are normalized before the reduced stickers are projected into a legal 3×3 state.

Reduction stages are transactional. Moves are first applied to a copy, stage-specific postconditions are checked, and invalid candidates are rejected rather than leaving a partially modified cube. The completed center, edge, parity, and Kociemba sequences are then replayed from the original scramble, and a result is returned only if the full NxN cube is solved.

The retained solution-length pass performs replay-verified commutation collapsing. It proves whether two moves commute by applying them in both orders to uniquely tagged cubes, safely reorders commuting turns so compatible moves become adjacent, combines their turn counts, and removes redundant commuting conjugates. Both the web animation and the displayed solution use the optimized sequence.

The current reduction path prioritizes deterministic completion, correctness, and low latency over move optimality. Independently realizing many orbit 3-cycles is reliable and fast, but produces substantially longer solutions than the near-optimal direct 3×3 path.

## Rejected experiments

Several optimizations were implemented and measured but excluded from the production path:

| Experiment | Measured result | Decision |
|---|---|---|
| Symmetry-compressed Phase 2 pruning tables | 7.78× table compression, but 24% slower solving | Rejected |
| Weighted NxN setup search with Dijkstra | 0.08% fewer moves with roughly 35% more latency | Rejected |
| Layered cost-aware NxN setup search | 0.45% fewer moves, about 7% more latency, and 8 of 40 solutions lengthened | Rejected |

The verified symmetry implementation remains in the repository as an artifact for possible future heuristic layouts. The weighted NxN setup experiments were fully reverted because their move savings were too small and inconsistent to justify the additional runtime.

## Build the C++ engine

Requirements: CMake 3.20+, Ninja, a C++20 compiler, and Python development headers for the bindings target.

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Run the native executable, unit tests, and benchmarks:

```bash
./build/nxn_cube
./build/tests/cube_tests
./build/benchmarks/cube_benchmarks
```

For a debug build:

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

## Run the web simulator locally

Build the Python extension, start the FastAPI service, and start Vite in separate terminals:

```bash
cmake --build build --target nxn_cube_py
source service/.venv/bin/activate
PYTHONPATH="$PWD/build/bindings" uvicorn app:app --app-dir service --reload --port 8000
```

```bash
cd web
npm ci
VITE_API_URL=http://127.0.0.1:8000 npm run dev
```

Open `http://localhost:5173`. Select a cube size, click **Scramble**, then **Solve**. Alternatively, enter a supported scramble and choose **Use Scramble**. The UI displays both scramble and solution notation in the format expected by Virtual Cube Net.

For a single-container deployment, the multi-stage `Dockerfile` builds the frontend and C++ extension, then serves the UI and API together on port 8000:

```bash
docker build -t nxn-cube .
docker run --rm -p 8000:8000 nxn-cube
```

## Verification

The native test suite covers cube moves, notation, coordinates, pruning table admissibility, IDA* behavior, parallel/thread-safe solving, NxN reduction stages, parity normalization, reduced-state projection, and full-solution replay. Python and FastAPI tests verify all supported sizes, validation failures, and scramble/solution round trips.

```bash
ctest --test-dir build --output-on-failure
```

## Next steps

Higher-potential general-N solution-length improvements remain deferred:

- choose lower-cost equivalent 3-cycle factorizations
- reorder consecutive cycles to maximize setup/undo cancellation
- batch multiple cycles under shared setup and undo sequences
- extend verified rewrites to combine parallel slices into wide moves
- investigate algorithms that solve corresponding pieces across multiple center or wing orbits simultaneously
- precompute and persist reusable orbit-action and conjugator tables for each supported N

Optimized candidates should always be replayed from the original cube and accepted only when they are both shorter and fully solving, preserving the current solver’s correctness guarantees.
