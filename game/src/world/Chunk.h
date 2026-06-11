#pragma once

#include <array>

#include "world/Block.h"

namespace vc {

// A 16^3 cube of blocks — the unit of meshing and (later) streaming.
// Flat array, YZX order: iterating x is contiguous, and a full horizontal
// layer is one contiguous slab (useful for lighting and heightmaps later).
class Chunk {
public:
    static constexpr int kSize = 16;
    static constexpr int kVolume = kSize * kSize * kSize;

    static constexpr bool InBounds(int x, int y, int z) {
        return x >= 0 && x < kSize && y >= 0 && y < kSize && z >= 0 && z < kSize;
    }

    static constexpr int Index(int x, int y, int z) { return (y * kSize + z) * kSize + x; }

    BlockId Get(int x, int y, int z) const { return m_blocks[Index(x, y, z)]; }
    void Set(int x, int y, int z, BlockId id) { m_blocks[Index(x, y, z)] = id; }

private:
    std::array<BlockId, kVolume> m_blocks{}; // zero-initialized: all air
};

} // namespace vc
