#pragma once

#include <array>
#include <cstdint>

#include "world/Chunk.h"

namespace vc {

// World height lives here (not World.h) so the light engine and mesher can
// reason about "above the world = full sky" without depending on World.
inline constexpr int kWorldHeightChunks = 4;
inline constexpr int kWorldHeightBlocks = kWorldHeightChunks * Chunk::kSize;

// Light levels are 0..15, Minecraft-style: sky light (15 = direct sky,
// attenuating under overhangs) and block light (from emissive blocks),
// packed as one byte per block. Computed off-thread per column and shared
// copy-on-write like block data.
class ChunkLight {
public:
    static constexpr uint8_t Pack(int sky, int block) {
        return static_cast<uint8_t>((sky << 4) | block);
    }
    static constexpr int Sky(uint8_t packed) { return packed >> 4; }
    static constexpr int Block(uint8_t packed) { return packed & 15; }

    uint8_t Get(int x, int y, int z) const { return m_data[Chunk::Index(x, y, z)]; }
    void Set(int x, int y, int z, uint8_t packed) { m_data[Chunk::Index(x, y, z)] = packed; }

    const std::array<uint8_t, Chunk::kVolume>& Raw() const { return m_data; }

private:
    std::array<uint8_t, Chunk::kVolume> m_data{}; // zero: no sky, no block light
};

} // namespace vc
