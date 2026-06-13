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
// by chunk.vert with matching bit offsets. This is the CUBIC stream: full
// blocks, cutout cubes (leaves), cross plants, and liquids — everything
// whose corners land on integer cell positions (plus the liquid yoff).
//   data0: x:5 | y:5 | z:5 | normal:3 | ao:2 | sky:4 | block:4 | (4 free)
//          x/y/z are chunk-local cell corners (0..16); normal indexes
//          BlockFace order; ao 0..3; light levels 0..15. Bits 28..31 are
//          unused (they once held torch sub-block insets; torches now use
//          the float model stream — see ModelVertex).
//   data1: u:5 | v:5 | layer:16 | yoff:4
//          UVs tile 0..16 across merged quads (sampler wraps). yoff is a
//          downward Y offset in ninths (0..9), subtracted from y in the
//          shader; 0 on ordinary faces. Used by liquid surface drops.
struct ChunkVertex {
    uint32_t data0;
    uint32_t data1;
};

// Fat MODEL vertex, 24 bytes — the second per-cell stream, for non-cube
// geometry (torches now; slabs/stairs/fences/panes later) whose corners
// and UVs need arbitrary fractional values the packed cubic vertex can't
// hold. Float position + float sub-tile UV, mirroring vanilla's
// DefaultVertexFormats.BLOCK and our fully-float entity path. Decoded by
// model_block.vert, which feeds the SAME varyings as chunk.vert so both
// streams share chunk.frag.
//   pos: chunk-local block units (cell + fraction), scaled by the per-draw
//        chunk transform like the cubic stream.
//   uv:  tile-space UV (1.0 = one tile); sub-rects sample within a tile.
//   packed: layer:16 | normal:3 | ao:4 | sky:4 | block:4.
struct ModelVertex {
    float x, y, z;
    float u, v;
    uint32_t packed;
};

// Quads only — 4 vertices each, drawn with the shared index pattern
// {0,1,2, 2,3,0} via vox::MeshPool, so no per-chunk index data exists.
// Three streams: opaque/cutout cubes + crosses, liquids (blended, drawn
// after opaque), and model boxes (alpha-tested, drawn with the opaque
// pass). Each goes to its own pool because the model stream's vertex
// layout differs.
struct ChunkMesh {
    std::vector<ChunkVertex> vertices;
    std::vector<ChunkVertex> transparentVertices;
    std::vector<ModelVertex> modelVertices;
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
