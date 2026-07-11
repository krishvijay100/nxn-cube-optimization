#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "solver/ida.h"

namespace {

// pre-generate scrambles once, deterministic seeds; we don't want scramble
// generation or move application inside the timed region
std::vector<cube::CubeState> make_scrambles(int count, int len) {
    std::vector<cube::CubeState> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        auto seq = cube::random_scramble(len, static_cast<uint64_t>(i + 1));
        cube::CubeState c = cube::solved_cube();
        for (cube::Move m : seq) cube::apply_move(c, m);
        out.push_back(c);
    }
    return out;
}

// warm the lazy-init move + pruning tables before any benchmark iterations
void warm() {
    static const bool _ = []() {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, cube::Move::R);
        (void)cube::solver::solve(c);
        return true;
    }();
    (void)_;
}

}  // namespace

static void BM_Solve(benchmark::State& state) {
    warm();
    auto scrambles = make_scrambles(64, 25);
    size_t i = 0;
    size_t total_moves = 0, p1_moves = 0, p2_moves = 0;
    for (auto _ : state) {
        auto p1 = cube::solver::ida_phase1(scrambles[i]);
        cube::CubeState intermediate = scrambles[i];
        for (cube::Move m : p1) cube::apply_move(intermediate, m);
        auto p2 = cube::solver::ida_phase2(intermediate);
        total_moves += p1.size() + p2.size();
        p1_moves    += p1.size();
        p2_moves    += p2.size();
        benchmark::DoNotOptimize(p1);
        benchmark::DoNotOptimize(p2);
        i = (i + 1) % scrambles.size();
    }
    state.SetItemsProcessed(state.iterations());
    state.counters["avg_moves"]    = benchmark::Counter(double(total_moves) / state.iterations());
    state.counters["avg_p1_moves"] = benchmark::Counter(double(p1_moves)    / state.iterations());
    state.counters["avg_p2_moves"] = benchmark::Counter(double(p2_moves)    / state.iterations());
}
BENCHMARK(BM_Solve)->Unit(benchmark::kMillisecond);

// isolated phase timings — length counters live on BM_Solve; these exist so we
// can watch each phase's time move independently during phase 5 optimization
static void BM_IdaPhase1(benchmark::State& state) {
    warm();
    auto scrambles = make_scrambles(64, 25);
    size_t i = 0;
    for (auto _ : state) {
        auto sol = cube::solver::ida_phase1(scrambles[i]);
        benchmark::DoNotOptimize(sol);
        i = (i + 1) % scrambles.size();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_IdaPhase1)->Unit(benchmark::kMillisecond);

static void BM_IdaPhase2(benchmark::State& state) {
    warm();
    auto scrambles = make_scrambles(64, 25);
    std::vector<cube::CubeState> g1_states;
    g1_states.reserve(scrambles.size());
    for (auto& c : scrambles) {
        cube::CubeState after = c;
        for (cube::Move m : cube::solver::ida_phase1(c)) cube::apply_move(after, m);
        g1_states.push_back(after);
    }
    size_t i = 0;
    for (auto _ : state) {
        auto sol = cube::solver::ida_phase2(g1_states[i]);
        benchmark::DoNotOptimize(sol);
        i = (i + 1) % g1_states.size();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_IdaPhase2)->Unit(benchmark::kMillisecond);
