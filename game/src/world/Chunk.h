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

    // M24 per-cell block metadata (orientation/facing; later slab-half,
    // stair-shape, log-axis). Block-specific meaning, like vanilla's block
    // state meta. 0 everywhere = no orientation (old saves, fresh worldgen).
    uint8_t GetMeta(int x, int y, int z) const { return m_meta[Index(x, y, z)]; }
    void SetMeta(int x, int y, int z, uint8_t meta) { m_meta[Index(x, y, z)] = meta; }

    const std::array<BlockId, kVolume>& Raw() const { return m_blocks; }
    std::array<BlockId, kVolume>& Raw() { return m_blocks; }
    const std::array<uint8_t, kVolume>& RawMeta() const { return m_meta; }
    std::array<uint8_t, kVolume>& RawMeta() { return m_meta; }

private:
    std::array<BlockId, kVolume> m_blocks{}; // zero-initialized: all air
    std::array<uint8_t, kVolume> m_meta{};   // M24: per-cell orientation/state
};

} // namespace vc
