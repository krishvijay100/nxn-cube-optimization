# nxn-cube-optimization

High-performance N×N Rubik's Cube solver in C++. Uses Reduction to solve the cube's centers and pair its edges (including handling parity cases) so it behaves like a 3×3, then applies Kociemba's two-phase algorithm to solve the remaining 3×3 state.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

```bash
./build/nxn_cube                    # main app
./build/tests/cube_tests            # unit tests
./build/benchmarks/cube_benchmarks  # benchmarks
```

## Debug build

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```
