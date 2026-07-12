#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "solver/ida.h"

// stress test: many threads concurrently solving different scrambles, to verify no shared writable state

namespace {

cube::CubeState apply_all(cube::CubeState c, const std::vector<cube::Move>& seq) {
    for (cube::Move m : seq) cube::apply_move(c, m);
    return c;
}

}  // namespace

TEST(IdaThreadSafety, ConcurrentSolvesAllSucceed) {
    constexpr int kThreads = 8;
    constexpr int kSolvesPerThread = 50;

    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([t, &failures]() {
            for (int i = 0; i < kSolvesPerThread; ++i) {
                // unique seed per (thread, iteration) so no two threads solve the
                // same scramble — every parallel solve has different work
                uint64_t seed = static_cast<uint64_t>(t) * 1000 + i + 1;
                auto scramble = cube::random_scramble(25, seed);
                cube::CubeState c = apply_all(cube::solved_cube(), scramble);

                auto solution = cube::solver::solve(c);
                cube::CubeState result = apply_all(c, solution);
                if (!cube::is_solved(result)) failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& w : workers) w.join();

    EXPECT_EQ(failures.load(), 0)
        << "some concurrent solves produced solutions that didn't solve their input";
}
