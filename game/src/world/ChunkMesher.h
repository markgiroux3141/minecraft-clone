#pragma once

#include <array>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "world/Chunk.h"

namespace vc {

struct ChunkVertex {
    glm::vec3 position; // chunk-local, [0,16]
    glm::vec3 normal;
    glm::vec2 uv;
    float layer; // texture-array layer
};

struct ChunkMesh {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;
};

// Immutable view of a chunk plus its face neighbors, captured on the main
// thread so meshing jobs never touch the (mutable) chunk map. Chunks are
// immutable after generation, so shared ownership is all the snapshotting
// we need; a null neighbor means air (world border or above/below).
struct ChunkSnapshot {
    std::shared_ptr<const Chunk> center;
    // Order matches BlockFace: +X, -X, +Y, -Y, +Z, -Z.
    std::array<std::shared_ptr<const Chunk>, 6> neighbors;
};

class ChunkMesher {
public:
    // Culled meshing: one quad per face that borders a non-opaque block.
    // Pure function of the snapshot — safe to call from worker threads
    // (BlockRegistry is read-only after startup registration). Greedy
    // merging lands in M5. Vertices are chunk-local; the renderer
    // translates by chunkCoord * 16.
    static ChunkMesh Build(const ChunkSnapshot& snapshot);
};

} // namespace vc
