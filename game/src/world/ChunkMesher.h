#pragma once

#include <array>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "world/Chunk.h"
#include "world/Light.h"
#include "world/Visibility.h"

namespace vc {

struct ChunkVertex {
    glm::vec3 position; // chunk-local, [0,16]
    glm::vec3 normal;
    glm::vec2 uv;       // tiles across merged quads (sampler wraps)
    float layer;        // texture-array layer
    float ao;           // ambient occlusion, 0 (fully occluded) .. 1 (open)
    float skyLight;     // 0..1 (level / 15)
    float blockLight;   // 0..1 (level / 15)
};

// Quads only — 4 vertices each, drawn with the shared index pattern
// {0,1,2, 2,3,0} via vox::MeshPool, so no per-chunk index data exists.
struct ChunkMesh {
    std::vector<ChunkVertex> vertices;
    VisibilityBits visibility = 0; // face connectivity for occlusion culling
};

// Immutable view of a chunk and its full 3x3x3 neighborhood (blocks and
// light), captured on the main thread so meshing jobs never touch the
// (mutable) chunk map. AO and smooth light need edge and corner
// neighbors, not just face neighbors. Chunk/light data is copy-on-write,
// so shared ownership is all the snapshotting we need; a null slot means
// air (only ever above/below the world — horizontal neighbors are gated).
struct ChunkSnapshot {
    std::array<std::shared_ptr<const Chunk>, 27> chunks;
    std::array<std::shared_ptr<const ChunkLight>, 27> light;
    bool skyAbove = false; // center is a top-of-world chunk: above = full sky

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
