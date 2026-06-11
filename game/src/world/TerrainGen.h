#pragma once

#include <glm/glm.hpp>

#include "world/Chunk.h"

namespace vc {

// Deterministic, stateless terrain generation: every call builds its own
// noise instance from the seed, so chunks can generate in any order (and,
// from M4, on any thread). Decoration (trees) is deterministic per world
// position, so every chunk independently regenerates the parts of any tree
// that reach into it.
class TerrainGenerator {
public:
    // Lowlands at or near this height get sand instead of grass; the M11
    // water pass fills up to it.
    static constexpr int kSeaLevel = 14;

    explicit TerrainGenerator(int seed) : m_seed(seed) {}

    void Generate(Chunk& chunk, const glm::ivec3& chunkCoord) const;

private:
    int m_seed;
};

} // namespace vc
