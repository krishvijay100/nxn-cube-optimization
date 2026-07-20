// scramble a 4x4, run stage-1 center reduction, print both sequences in WCA notation, so that i can execute them on a physical cube and verify centers end up monochromatic

#include <iostream>
#include <string>

#include "cube_nxn/cube_nxn.h"

using namespace cube_nxn;

namespace {

std::string join_moves(const std::vector<Move>& moves) {
    std::string out;
    for (size_t i = 0; i < moves.size(); ++i) {
        if (i > 0) out += ' ';
        out += format_move(moves[i]);
    }
    return out;
}

std::string join_steps(const std::vector<MoveStep>& steps) {
    std::string out;
    bool first = true;
    for (const auto& step : steps) {
        for (const auto& m : step) {
            if (!first) out += ' ';
            out += format_move(m);
            first = false;
        }
    }
    return out;
}

int total_move_count(const std::vector<MoveStep>& steps) {
    int total = 0;
    for (const auto& s : steps) total += static_cast<int>(s.size());
    return total;
}

}  // namespace

int main(int argc, char** argv) {
    uint64_t seed = 1;
    int scramble_len = 8;
    if (argc >= 2) seed = static_cast<uint64_t>(std::stoull(argv[1]));
    if (argc >= 3) scramble_len = std::stoi(argv[2]);

    NxNCube cube(4);
    auto scramble = random_scramble(4, scramble_len, seed);
    for (const auto& m : scramble) apply_move(cube, m);

    std::cout << "Seed:      " << seed << "\n";
    std::cout << "Scramble:  " << join_moves(scramble) << "\n";
    std::cout << "           (" << scramble.size() << " moves)\n\n";

    auto reduction = solve_centers_n4(cube);
    if (reduction.empty()) {
        std::cout << "!! solve_centers_n4 returned empty — solver failure\n";
        return 1;
    }

    // flatten to raw moves and collapse redundant same-layer runs
    std::vector<Move> raw;
    for (const auto& s : reduction) for (const auto& m : s) raw.push_back(m);
    auto collapsed = collapse_redundant_moves(raw);

    std::cout << "Solution (raw):       " << join_moves(raw) << "\n";
    std::cout << "                      (" << raw.size() << " moves)\n\n";
    std::cout << "Solution (collapsed): " << join_moves(collapsed) << "\n";
    std::cout << "                      (" << collapsed.size() << " moves, "
              << (raw.size() - collapsed.size()) << " removed)\n";

    return 0;
}