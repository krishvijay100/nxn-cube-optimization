#include <gtest/gtest.h>

#include "solver/pruning.h"


TEST(SymPruning, CornerCompressedMatchesRaw) {
    using namespace cube::solver;
    const auto& raw = corner_perm_slice_perm_pruning();
    const auto& sym = sym_corner_perm_slice_perm_pruning();
    for (Coord cp = 0; cp < CORNER_PERM_COORDS; ++cp) {
        for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp) {
            Distance rawd = raw[cp][sp];
            Distance symd = sym.dist[sym.class_of[cp][sp]];
            ASSERT_EQ(rawd, symd) << "mismatch at cp=" << cp << " sp=" << sp;
        }
    }
}

TEST(SymPruning, EdgeCompressedMatchesRaw) {
    using namespace cube::solver;
    const auto& raw = edge_perm_slice_perm_pruning();
    const auto& sym = sym_edge_perm_slice_perm_pruning();
    for (Coord ep = 0; ep < EDGE_PERM_UD_COORDS; ++ep) {
        for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp) {
            Distance rawd = raw[ep][sp];
            Distance symd = sym.dist[sym.class_of[ep][sp]];
            ASSERT_EQ(rawd, symd) << "mismatch at ep=" << ep << " sp=" << sp;
        }
    }
}

TEST(SymPruning, CornerCompressionRatioReported) {
    using namespace cube::solver;
    const auto& sym = sym_corner_perm_slice_perm_pruning();
    size_t total_pairs = size_t(CORNER_PERM_COORDS) * SLICE_PERM_COORDS;
    size_t classes = sym.dist.size();
    std::cout << "[  INFO   ] corner sym_pruning: " << classes
              << " classes / " << total_pairs << " pairs = "
              << (double(total_pairs) / double(classes)) << "x compression, "
              << (classes * sizeof(Distance)) << " bytes for dist" << std::endl;
    EXPECT_GT(classes, 0u);
    EXPECT_LE(classes, total_pairs);
}

TEST(SymPruning, EdgeCompressionRatioReported) {
    using namespace cube::solver;
    const auto& sym = sym_edge_perm_slice_perm_pruning();
    size_t total_pairs = size_t(EDGE_PERM_UD_COORDS) * SLICE_PERM_COORDS;
    size_t classes = sym.dist.size();
    std::cout << "[  INFO   ] edge sym_pruning: " << classes
              << " classes / " << total_pairs << " pairs = "
              << (double(total_pairs) / double(classes)) << "x compression, "
              << (classes * sizeof(Distance)) << " bytes for dist" << std::endl;
    EXPECT_GT(classes, 0u);
    EXPECT_LE(classes, total_pairs);
}
