#include "world/TerrainGen.h"

#include <algorithm>

#include <FastNoiseLite.h>

#include "world/Light.h" // kWorldHeightBlocks

namespace vc {

namespace {

// Tree placement: the world is tiled into kTreeCell^2-column cells; each
// cell hash-decides whether it hosts a tree and where inside the cell it
// stands. Cell-based placement keeps trees naturally spaced and lets a
// chunk enumerate exactly the cells whose canopy could reach it.
constexpr int kTreeCell = 8;
constexpr float kTreeChance = 0.45f;
constexpr int kCanopyRadius = 2;
// Beach band: columns this close to sea level get sand and no trees.
constexpr int kBeachTop = TerrainGenerator::kSeaLevel + 2;

uint32_t Mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

uint32_t Hash(int seed, int x, int z, int salt) {
    return Mix(static_cast<uint32_t>(seed) ^ Mix(static_cast<uint32_t>(x) * 0x9E3779B9u ^
                                                 static_cast<uint32_t>(z) * 0x85EBCA6Bu ^
                                                 static_cast<uint32_t>(salt) * 0xC2B2AE35u));
}

float Hash01(int seed, int x, int z, int salt) {
    return static_cast<float>(Hash(seed, x, z, salt) >> 8) / 16777216.0f;
}

int FloorDiv(int a, int b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// Writes a world-space block into this chunk if the cell is inside it AND
// still air — decoration never carves terrain or other trees' blocks.
void SetIfAir(Chunk& chunk, const glm::ivec3& origin, int wx, int wy, int wz, BlockId id) {
    const int lx = wx - origin.x;
    const int ly = wy - origin.y;
    const int lz = wz - origin.z;
    if (lx < 0 || lx >= Chunk::kSize || ly < 0 || ly >= Chunk::kSize || lz < 0 ||
        lz >= Chunk::kSize) {
        return;
    }
    if (chunk.Get(lx, ly, lz) == blocks::Air) {
        chunk.Set(lx, ly, lz, id);
    }
}

// Classic oak: trunk, two radius-2 leaf layers (minus corners) around the
// trunk top, a 3x3 above, and a plus-shaped cap.
void PlaceTree(Chunk& chunk, const glm::ivec3& origin, int tx, int ground, int tz, int trunk) {
    // The dirt patch under the trunk (replaces the grass surface).
    const int lx = tx - origin.x;
    const int ly = ground - origin.y;
    const int lz = tz - origin.z;
    if (lx >= 0 && lx < Chunk::kSize && ly >= 0 && ly < Chunk::kSize && lz >= 0 &&
        lz < Chunk::kSize && chunk.Get(lx, ly, lz) == blocks::Grass) {
        chunk.Set(lx, ly, lz, blocks::Dirt);
    }

    const int top = ground + trunk;
    for (int wy = ground + 1; wy <= top; ++wy) {
        SetIfAir(chunk, origin, tx, wy, tz, blocks::Log);
    }
    for (int wy = top - 1; wy <= top; ++wy) {
        for (int dz = -kCanopyRadius; dz <= kCanopyRadius; ++dz) {
            for (int dx = -kCanopyRadius; dx <= kCanopyRadius; ++dx) {
                if (std::abs(dx) == kCanopyRadius && std::abs(dz) == kCanopyRadius) {
                    continue; // clipped corners
                }
                SetIfAir(chunk, origin, tx + dx, wy, tz + dz, blocks::Leaves);
            }
        }
    }
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            SetIfAir(chunk, origin, tx + dx, top + 1, tz + dz, blocks::Leaves);
        }
    }
    SetIfAir(chunk, origin, tx - 1, top + 2, tz, blocks::Leaves);
    SetIfAir(chunk, origin, tx + 1, top + 2, tz, blocks::Leaves);
    SetIfAir(chunk, origin, tx, top + 2, tz - 1, blocks::Leaves);
    SetIfAir(chunk, origin, tx, top + 2, tz + 1, blocks::Leaves);
    SetIfAir(chunk, origin, tx, top + 2, tz, blocks::Leaves);
}

} // namespace

void TerrainGenerator::Generate(Chunk& chunk, const glm::ivec3& chunkCoord) const {
    FastNoiseLite noise(m_seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(4);
    noise.SetFrequency(0.008f);

    const auto heightAt = [&](int wx, int wz) {
        return 24 + static_cast<int>(
                        20.0f * noise.GetNoise(static_cast<float>(wx), static_cast<float>(wz)));
    };

    const glm::ivec3 origin = chunkCoord * Chunk::kSize;

    for (int z = 0; z < Chunk::kSize; ++z) {
        for (int x = 0; x < Chunk::kSize; ++x) {
            const int height = heightAt(origin.x + x, origin.z + z);
            const bool beach = height <= kBeachTop;

            for (int y = 0; y < Chunk::kSize; ++y) {
                const int wy = origin.y + y;
                if (wy > height) {
                    if (wy > kSeaLevel) {
                        break; // air above surface and sea; chunk is air-initialized
                    }
                    chunk.Set(x, y, z, blocks::Water);
                    continue;
                }
                BlockId id = blocks::Stone;
                if (beach) {
                    if (wy >= height - 2) {
                        id = blocks::Sand;
                    }
                } else if (wy == height) {
                    id = blocks::Grass;
                } else if (wy >= height - 3) {
                    id = blocks::Dirt;
                }
                chunk.Set(x, y, z, id);
            }
        }
    }

    // Decoration: every tree whose canopy can reach this chunk. Cells are
    // visited in the same (ascending) order from every chunk, so overlapping
    // trees resolve identically no matter which chunk generates first.
    const int minCellX = FloorDiv(origin.x - kCanopyRadius, kTreeCell);
    const int maxCellX = FloorDiv(origin.x + Chunk::kSize - 1 + kCanopyRadius, kTreeCell);
    const int minCellZ = FloorDiv(origin.z - kCanopyRadius, kTreeCell);
    const int maxCellZ = FloorDiv(origin.z + Chunk::kSize - 1 + kCanopyRadius, kTreeCell);

    for (int cz = minCellZ; cz <= maxCellZ; ++cz) {
        for (int cx = minCellX; cx <= maxCellX; ++cx) {
            if (Hash01(m_seed, cx, cz, 1) > kTreeChance) {
                continue;
            }
            const int tx = cx * kTreeCell + static_cast<int>(Hash(m_seed, cx, cz, 2) % kTreeCell);
            const int tz = cz * kTreeCell + static_cast<int>(Hash(m_seed, cx, cz, 3) % kTreeCell);
            const int ground = heightAt(tx, tz);
            if (ground <= kBeachTop) {
                continue; // grass only — no beach or underwater trees
            }
            const int trunk = 4 + static_cast<int>(Hash(m_seed, cx, cz, 4) % 3); // 4..6
            if (ground + trunk + 2 >= kWorldHeightBlocks) {
                continue;
            }
            PlaceTree(chunk, origin, tx, ground, tz, trunk);
        }
    }
}

} // namespace vc
