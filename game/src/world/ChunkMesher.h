#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "world/Chunk.h"
#include "world/Light.h"
#include "world/Visibility.h"

namespace vc {

// Packed chunk vertex, 8 bytes — every field is a small integer. Decoded
// by chunk.vert with matching bit offsets:
//   data0: x:5 | y:5 | z:5 | normal:3 | ao:2 | sky:4 | block:4 | xIn:2 | zIn:2
//          x/y/z are chunk-local cell corners (0..16); normal indexes
//          BlockFace order; ao 0..3; light levels 0..15. xIn/zIn are
//          sub-block insets added to x/z in the shader (0 = none,
//          1 = +7/16, 2 = +9/16) — torch planes only, zero elsewhere.
//   data1: u:5 | v:5 | layer:16 | drop:4
//          UVs tile 0..16 across merged quads (sampler wraps). drop is
//          the liquid surface's distance below the cell top in ninths
//          (0..9), subtracted from y in the shader; 0 on solid faces.
struct ChunkVertex {
    uint32_t data0;
    uint32_t data1;
};

// Quads only — 4 vertices each, drawn with the shared index pattern
// {0,1,2, 2,3,0} via vox::MeshPool, so no per-chunk index data exists.
// Liquids (water) land in the separate transparent stream, drawn blended
// after all opaque geometry.
struct ChunkMesh {
    std::vector<ChunkVertex> vertices;
    std::vector<ChunkVertex> transparentVertices;
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
