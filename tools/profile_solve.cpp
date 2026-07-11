// hot-loop harness for attaching a profiler (Instruments Time Profiler etc.)
// prints a solve count every second so you can see it's running

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "solver/ida.h"

int main() {
    // pre-generate a batch of scrambles so profile samples land on solve code,
    // not RNG / move-application code
    constexpr int NUM_SCRAMBLES = 128;
    std::vector<cube::CubeState> scrambles;
    scrambles.reserve(NUM_SCRAMBLES);
    for (int i = 0; i < NUM_SCRAMBLES; ++i) {
        auto seq = cube::random_scramble(25, static_cast<uint64_t>(i + 1));
        cube::CubeState c = cube::solved_cube();
        for (cube::Move m : seq) cube::apply_move(c, m);
        scrambles.push_back(c);
    }

    // warm the lazy-init tables outside the timed loop
    (void)cube::solver::solve(scrambles[0]);

    std::printf("attach profiler now, then press enter to start\n");
    std::fflush(stdout);
    std::getchar();

    std::printf("running... ctrl-c to stop\n");
    std::fflush(stdout);

    auto last_report = std::chrono::steady_clock::now();
    std::uint64_t solves = 0, solves_since_report = 0;
    std::size_t i = 0;
    while (true) {
        auto sol = cube::solver::solve(scrambles[i]);
        (void)sol;
        i = (i + 1) % scrambles.size();
        ++solves;
        ++solves_since_report;

        auto now = std::chrono::steady_clock::now();
        if (now - last_report >= std::chrono::seconds(1)) {
            std::printf("solves total=%llu, this sec=%llu\n",
                        (unsigned long long)solves,
                        (unsigned long long)solves_since_report);
            std::fflush(stdout);
            solves_since_report = 0;
            last_report = now;
        }
    }
}
