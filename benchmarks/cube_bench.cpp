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

static void BM_ApplySingleMove(benchmark::State& state) {
    cube::CubeState c = cube::solved_cube();
    for (auto _ : state) {
        cube::apply_move(c, cube::Move::R);
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_ApplySingleMove);

static constexpr cube::Move BENCH_SCRAMBLE[] = {
    cube::Move::R,  cube::Move::U,  cube::Move::F2, cube::Move::D_prime,
    cube::Move::L,  cube::Move::B,  cube::Move::U2, cube::Move::R_prime,
    cube::Move::F,  cube::Move::D,  cube::Move::L2, cube::Move::B_prime,
    cube::Move::U_prime, cube::Move::R2, cube::Move::F_prime, cube::Move::L_prime,
    cube::Move::B2, cube::Move::D2,
};

static void BM_ApplyScrambleSequence(benchmark::State& state) {
    for (auto _ : state) {
        cube::CubeState c = cube::solved_cube();
        for (cube::Move m : BENCH_SCRAMBLE) {
            cube::apply_move(c, m);
        }
        benchmark::DoNotOptimize(c);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(std::size(BENCH_SCRAMBLE)));
}
BENCHMARK(BM_ApplyScrambleSequence);
