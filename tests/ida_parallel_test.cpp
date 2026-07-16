#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "solver/ida.h"

namespace {

cube::CubeState apply_all(cube::CubeState c, const std::vector<cube::Move>& seq) {
    for (cube::Move m : seq) cube::apply_move(c, m);
    return c;
}

}  // namespace

TEST(IdaParallel, MatchesSerialLength) {
    using namespace cube;
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        auto scramble = random_scramble(25, seed);
        CubeState c = apply_all(solved_cube(), scramble);

        auto serial_sol = cube::solver::solve(c);
        auto par_sol    = cube::solver::solve_parallel(c, 4);

        ASSERT_EQ(par_sol.size(), serial_sol.size())
            << "parallel returned a non-optimal length for seed=" << seed
            << " (serial=" << serial_sol.size() << ", parallel=" << par_sol.size() << ")";

        CubeState after = apply_all(c, par_sol);
        EXPECT_TRUE(is_solved(after)) << "parallel solution didn't solve for seed=" << seed;
    }
}

// falling back to num_threads=1 must be identical to plain solve()
TEST(IdaParallel, SingleThreadFallsBackToSerial) {
    using namespace cube;
    for (uint64_t seed = 1; seed <= 5; ++seed) {
        auto scramble = random_scramble(20, seed);
        CubeState c = apply_all(solved_cube(), scramble);

        auto serial = cube::solver::solve(c);
        auto par1   = cube::solver::solve_parallel(c, 1);
        EXPECT_EQ(par1.size(), serial.size());
        EXPECT_TRUE(is_solved(apply_all(c, par1)));
    }
}

TEST(IdaParallel, ConcurrentParallelSolvesAllSucceed) {
    using namespace cube;
    constexpr int kOuter = 4;
    constexpr int kPerOuter = 25;
    constexpr int kInnerThreads = 4;

    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    workers.reserve(kOuter);
    for (int t = 0; t < kOuter; ++t) {
        workers.emplace_back([t, &failures]() {
            for (int i = 0; i < kPerOuter; ++i) {
                uint64_t seed = uint64_t(t) * 10000 + i + 1;
                auto scramble = random_scramble(20, seed);
                CubeState c = apply_all(solved_cube(), scramble);
                auto sol = cube::solver::solve_parallel(c, kInnerThreads);
                if (!is_solved(apply_all(c, sol))) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& w : workers) w.join();
    EXPECT_EQ(failures.load(), 0);
}

TEST(IdaParallel, SolvedCubeReturnsEmpty) {
    auto sol = cube::solver::solve_parallel(cube::solved_cube(), 4);
    EXPECT_TRUE(sol.empty());
}
