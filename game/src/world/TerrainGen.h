#pragma once

#include <glm/glm.hpp>

#include "world/Chunk.h"

namespace vc {

// Deterministic, stateless terrain generation: every call builds its own
// noise instance from the seed, so chunks can generate in any order (and,
// from M4, on any thread).
class TerrainGenerator {
public:
    explicit TerrainGenerator(int seed) : m_seed(seed) {}

    void Generate(Chunk& chunk, const glm::ivec3& chunkCoord) const;

private:
    int m_seed;
};

} // namespace vc
