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
    // water pass fills up to it. M25 raised this to vanilla's 63 (was 14)
    // when the world grew to 128 tall — surface now sits at ~y65 like
    // vanilla, with the full underground below it.
    static constexpr int kSeaLevel = 63;

    explicit TerrainGenerator(int seed) : m_seed(seed) {}

    void Generate(Chunk& chunk, const glm::ivec3& chunkCoord) const;

private:
    int m_seed;
};

} // namespace vc
