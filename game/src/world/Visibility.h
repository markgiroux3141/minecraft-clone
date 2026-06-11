#pragma once

#include <cstdint>

#include "world/Block.h"

namespace vc {

// Chunk occlusion culling ("cave culling"): every chunk stores which pairs
// of its 6 faces are connected by a path of non-opaque blocks, as one bit
// per unordered face pair (6 choose 2 = 15 bits). The renderer BFS-walks
// chunk to chunk from the camera and only descends through connected
// pairs, so chunks sealed behind terrain are never drawn. Face indices
// follow BlockFace (PosX, NegX, PosY, NegY, PosZ, NegZ).
using VisibilityBits = uint16_t;

inline constexpr VisibilityBits kAllFacesConnected = 0x7FFF;

constexpr int OppositeFace(int face) {
    return face ^ 1; // BlockFace pairs faces as +axis, -axis
}

// Bit index for the unordered pair (a, b), a != b: row-major over the
// upper triangle of the 6x6 face matrix.
constexpr int FacePairBit(int a, int b) {
    const int lo = a < b ? a : b;
    const int hi = a < b ? b : a;
    return 5 * lo - lo * (lo - 1) / 2 + (hi - lo - 1);
}

constexpr bool FacesConnected(VisibilityBits bits, int a, int b) {
    return (bits >> FacePairBit(a, b)) & 1;
}

static_assert(FacePairBit(0, 1) == 0 && FacePairBit(4, 5) == 14);
static_assert(OppositeFace(static_cast<int>(BlockFace::PosY)) ==
              static_cast<int>(BlockFace::NegY));

} // namespace vc
