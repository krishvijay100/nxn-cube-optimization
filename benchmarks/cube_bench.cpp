#include <benchmark/benchmark.h>
#include "cube/cube.h"

static void BM_PlaceholderAdd(benchmark::State& state) {
    for (auto _ : state) {
        int result = cube::placeholder_add(2, 3);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_PlaceholderAdd);
