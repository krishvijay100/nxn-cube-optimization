#include "cube_nxn/reduction_general.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <map>
#include <queue>
#include <unordered_set>

namespace cube_nxn {

namespace {

CenterFlavor classify_center(int dr, int dc) {
    const int adr = std::abs(dr);
    const int adc = std::abs(dc);
    if (dr == 0 && dc == 0) return CenterFlavor::Fixed;
    if (dr == 0 || dc == 0) return CenterFlavor::PlusCenter;
    if (adr == adc)         return CenterFlavor::XCenter;
    return CenterFlavor::Oblique;
}

int center_ring(int dr, int dc) {
    const int adr = std::abs(dr);
    const int adc = std::abs(dc);
    return (adr > adc) ? adr : adc;
}

}

std::vector<CenterSlot> enumerate_center_slots(int n) {
    assert(n >= 3);
    std::vector<CenterSlot> out;
    for (int f = 0; f < NUM_FACES; ++f) {
        for (int r = 1; r <= n - 2; ++r) {
            for (int c = 1; c <= n - 2; ++c) {
                const int dr = 2 * r - (n - 1);
                const int dc = 2 * c - (n - 1);
                out.push_back(CenterSlot{
                    static_cast<Face>(f), r, c,
                    classify_center(dr, dc), center_ring(dr, dc),
                });
            }
        }
    }
    return out;
}

std::vector<EdgeSlotG> enumerate_edge_slots(int n) {
    assert(n >= 3);
    std::vector<EdgeSlotG> out;
    const int wing_pairs = (n - 2) / 2;
    const bool has_middle = (n % 2 == 1);
    for (int edge_id = 0; edge_id < 12; ++edge_id) {
        for (int w = 0; w < wing_pairs; ++w) {
            for (int side = 0; side < 2; ++side) {
                out.push_back(EdgeSlotG{edge_id, EdgeFlavor::Wing, w, side});
            }
        }
        if (has_middle) {
            out.push_back(EdgeSlotG{edge_id, EdgeFlavor::Middle, -1, 0});
        }
    }
    return out;
}
}  // namespace cube_nxn
