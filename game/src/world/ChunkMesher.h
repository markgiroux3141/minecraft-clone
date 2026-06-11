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
    glm::vec2 uv;       // tiles across merged quads (sampler wraps)
    float layer;        // texture-array layer
    float ao;           // ambient occlusion, 0 (fully occluded) .. 1 (open)
};

struct ChunkMesh {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;
};

// Immutable view of a chunk and its full 3x3x3 neighborhood, captured on
// the main thread so meshing jobs never touch the (mutable) chunk map.
// AO needs edge and corner neighbors, not just face neighbors. Chunks are
// immutable after generation, so shared ownership is all the snapshotting
// we need; a null slot means air (world border or above/below).
struct ChunkSnapshot {
    std::array<std::shared_ptr<const Chunk>, 27> chunks;

    // d* in [0,2]; the center chunk lives at Index(1,1,1).
    static constexpr int Index(int dx, int dy, int dz) { return (dy * 3 + dz) * 3 + dx; }
};

class ChunkMesher {
public:
    // Greedy meshing: faces with the same texture layer and AO merge into
    // maximal rectangles, with UVs tiling across the quad. AO is the
    // classic 3-neighbor corner term, baked per vertex; quads flip their
    // diagonal to interpolate AO without anisotropy artifacts.
    // Pure function of the snapshot — safe to call from worker threads
    // (BlockRegistry is read-only after startup registration). Vertices
    // are chunk-local; the renderer translates by chunkCoord * 16.
    static ChunkMesh Build(const ChunkSnapshot& snapshot);
};

} // namespace vc
