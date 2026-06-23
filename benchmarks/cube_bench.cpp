#include <benchmark/benchmark.h>
#include "cube/cube.h"

static void BM_SolvedCubeConstruction(benchmark::State& state) {
    for (auto _ : state) {
        cube::CubeState c = cube::solved_cube();
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_SolvedCubeConstruction);

static void BM_IsSolvedCheck(benchmark::State& state) {
    cube::CubeState c = cube::solved_cube();
    for (auto _ : state) {
        bool result = cube::is_solved(c);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_IsSolvedCheck);
