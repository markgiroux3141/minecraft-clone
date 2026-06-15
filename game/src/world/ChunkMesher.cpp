#include "world/ChunkMesher.h"

#include <algorithm>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assert.h"

namespace vc {

namespace {

// The chunk plus a one-block shell of its neighbors, flattened for O(1)
// lookups during meshing. ~24 KB, lives on the worker's stack.
constexpr int kPad = Chunk::kSize + 2;
constexpr int kPadVolume = kPad * kPad * kPad;

constexpr int PadIndex(int x, int y, int z) {
    return ((y + 1) * kPad + (z + 1)) * kPad + (x + 1);
}

struct PaddedVolume {
    std::array<BlockId, kPadVolume> id;
    std::array<uint8_t, kPadVolume> meta; // M24 per-cell orientation/state
    std::array<uint8_t, kPadVolume> opaque;
    std::array<uint8_t, kPadVolume> cutout; // alpha-tested cubes (leaves)
    std::array<uint8_t, kPadVolume> cross;  // plants: X-shaped quad pairs
    std::array<uint8_t, kPadVolume> model;  // non-cube box models (torches)
    std::array<uint8_t, kPadVolume> liquid;
    std::array<uint8_t, kPadVolume> level; // liquidLevel (8 source, 7..1 flow)
    std::array<uint8_t, kPadVolume> light; // packed sky<<4 | block
};

void FillPadded(const ChunkSnapshot& snapshot, PaddedVolume& out) {
    const auto& registry = BlockRegistry::Get();
    for (int y = -1; y <= Chunk::kSize; ++y) {
        for (int z = -1; z <= Chunk::kSize; ++z) {
            for (int x = -1; x <= Chunk::kSize; ++x) {
                // Arithmetic shift maps -1 -> slot 0, 0..15 -> 1, 16 -> 2.
                const int dy = (y >> 4) + 1;
                const int slot = ChunkSnapshot::Index((x >> 4) + 1, dy, (z >> 4) + 1);
                const Chunk* chunk = snapshot.chunks[slot].get();
                const BlockId id = chunk ? chunk->Get(x & 15, y & 15, z & 15) : blocks::Air;
                const int p = PadIndex(x, y, z);
                const BlockDef& def = registry.Def(id);
                out.id[p] = id;
                out.meta[p] = chunk ? chunk->GetMeta(x & 15, y & 15, z & 15) : 0;
                out.opaque[p] = def.opaque ? 1 : 0;
                out.cutout[p] = def.cutout ? 1 : 0;
                out.cross[p] = def.cross ? 1 : 0;
                // M28: slabs/stairs also emit into the model stream (their
                // boxes are built per-cell from meta in EmitModelCell).
                out.model[p] = (!def.model.empty() || def.slab || def.stairs) ? 1 : 0;
                out.liquid[p] = def.liquid ? 1 : 0;
                out.level[p] = def.liquidLevel;

                const ChunkLight* light = snapshot.light[slot].get();
                if (light) {
                    out.light[p] = light->Get(x & 15, y & 15, z & 15);
                } else {
                    // Only above/below the world (horizontal neighbors are
                    // gated): above the top chunk is full sky, below is dark.
                    out.light[p] =
                        (dy == 2 && snapshot.skyAbove) ? ChunkLight::Pack(15, 0) : 0;
                }
            }
        }
    }
}

// Per-face slicing basis. Order matches BlockFace. (uAxis, vAxis) are
// chosen so cross(u, v) == outward normal, which makes the emitted corner
// order (0,0) (w,0) (w,h) (0,h) wind CCW viewed from outside. That choice
// puts world-up along the mask's u axis on +X/-Z, so those faces swap UV
// components to keep texture "up" (grass band) aligned with world up.
struct FaceBasis {
    int n;       // axis the face is perpendicular to
    int s;       // +1 or -1: which side of the block
    int uAxis;   // mask "u" direction (quad width)
    int vAxis;   // mask "v" direction (quad height)
    bool swapUv; // map mask (u,v) -> texture (t,s) instead of (s,t)
};

constexpr FaceBasis kFaces[6] = {
    {0, +1, 1, 2, true},  // +X
    {0, -1, 2, 1, false}, // -X
    {1, +1, 2, 0, false}, // +Y
    {1, -1, 0, 2, false}, // -Y
    {2, +1, 0, 1, false}, // +Z
    {2, -1, 1, 0, true},  // -Z
};

// Per-corner shading sampled from the four cells touching the vertex in
// the plane one step out along the face normal: the classic AO term plus
// smooth (averaged) sky/block light over the transparent cells.
struct CornerSample {
    int ao;    // 0..3
    int sky;   // 0..15
    int block; // 0..15
};

CornerSample SampleCorner(const PaddedVolume& vol, const int cell[3], const FaceBasis& fb,
                          int offU, int offV) {
    int above[3] = {cell[0], cell[1], cell[2]};
    above[fb.n] += fb.s;
    const auto cellAt = [&](int du, int dv) {
        int p[3] = {above[0], above[1], above[2]};
        p[fb.uAxis] += du;
        p[fb.vAxis] += dv;
        return PadIndex(p[0], p[1], p[2]);
    };

    const int center = cellAt(0, 0);
    const int side1 = cellAt(offU, 0);
    const int side2 = cellAt(0, offV);
    const int corner = cellAt(offU, offV);

    const bool s1 = vol.opaque[side1] != 0;
    const bool s2 = vol.opaque[side2] != 0;
    const bool c = vol.opaque[corner] != 0;

    CornerSample sample{};
    sample.ao = (s1 && s2) ? 0
                           : 3 - (static_cast<int>(s1) + static_cast<int>(s2) +
                                  static_cast<int>(c));

    // Smooth light: average over the transparent cells only — opaque
    // neighbors don't drag the average down (AO already darkens corners).
    int skySum = 0;
    int blockSum = 0;
    int count = 0;
    const int cells[4] = {center, side1, side2, corner};
    for (const int p : cells) {
        if (vol.opaque[p]) {
            continue;
        }
        skySum += ChunkLight::Sky(vol.light[p]);
        blockSum += ChunkLight::Block(vol.light[p]);
        ++count;
    }
    if (count > 0) {
        sample.sky = (skySum + count / 2) / count;
        sample.block = (blockSum + count / 2) / count;
    }
    return sample;
}

// M24: texture layer of `face` for a possibly-oriented cube. Most blocks
// just return faceTiles[face]; a horizontalFacing block (furnace/table)
// shows its canonical front tile (faceTiles[PosX]) on whatever horizontal
// face its meta names and the side tile (faceTiles[NegX]) on the other
// three. Top/bottom are unaffected. Differently-facing neighbors get
// different keys here, so the greedy merge keeps them apart naturally.
uint16_t OrientedFaceTile(const BlockDef& def, uint8_t meta, int face) {
    if (!def.horizontalFacing || face == static_cast<int>(BlockFace::PosY) ||
        face == static_cast<int>(BlockFace::NegY)) {
        return def.faceTiles[face];
    }
    const int front = static_cast<int>(meta); // meta 0 = PosX (pre-M24 default)
    return def.faceTiles[face == front ? static_cast<int>(BlockFace::PosX)
                                       : static_cast<int>(BlockFace::NegX)];
}

// Mask cell key (uint64), opaque faces only — liquids emit per cell, see
// EmitLiquidCell: bit 56 = face present, bits 40..55 = corner block light
// (4x4), bits 24..39 = corner sky light (4x4), bits 8..23 = texture
// layer, bits 0..7 = corner AO (4x2). Corners are stored in emitted
// order. Cells merge only on exact key match, which keeps both AO and
// light gradients continuous across merged quads.
uint64_t MaskKey(uint32_t layer, const CornerSample corners[4]) {
    uint64_t key = uint64_t{1} << 56;
    key |= uint64_t{layer} << 8;
    for (int k = 0; k < 4; ++k) {
        key |= static_cast<uint64_t>(corners[k].ao) << (6 - k * 2);
        key |= static_cast<uint64_t>(corners[k].sky) << (24 + k * 4);
        key |= static_cast<uint64_t>(corners[k].block) << (40 + k * 4);
    }
    return key;
}

void EmitQuad(ChunkMesh& mesh, int face, const FaceBasis& fb, int slice, int u0, int v0, int w,
              int h, uint64_t key) {
    auto& vertices = mesh.vertices;
    const int plane = slice + (fb.s > 0 ? 1 : 0);
    const auto layer = static_cast<uint32_t>((key >> 8) & 0xFFFFu);
    uint32_t ao[4];
    uint32_t sky[4];
    uint32_t block[4];
    for (int k = 0; k < 4; ++k) {
        ao[k] = static_cast<uint32_t>(key >> (6 - k * 2)) & 3u;
        sky[k] = static_cast<uint32_t>(key >> (24 + k * 4)) & 15u;
        block[k] = static_cast<uint32_t>(key >> (40 + k * 4)) & 15u;
    }
    const int cu[4] = {0, w, w, 0};
    const int cv[4] = {0, 0, h, h};

    // Every quad uses the implicit index pattern {0,1,2, 2,3,0} (see
    // World's MeshPool). Splitting along the brighter AO diagonal (no
    // dark-cross anisotropy artifact) is done by rotating the emission
    // order one step — a cyclic rotation yields exactly the triangles of
    // the flipped diagonal with the winding preserved.
    const int rotate = ao[0] + ao[2] >= ao[1] + ao[3] ? 0 : 1;
    for (int j = 0; j < 4; ++j) {
        const int k = (j + rotate) & 3;
        int pos[3];
        pos[fb.n] = plane;
        pos[fb.uAxis] = u0 + cu[k];
        pos[fb.vAxis] = v0 + cv[k];
        const int u = fb.swapUv ? cv[k] : cu[k];
        const int v = fb.swapUv ? cu[k] : cv[k];
        vertices.push_back({
            static_cast<uint32_t>(pos[0]) | static_cast<uint32_t>(pos[1]) << 5 |
                static_cast<uint32_t>(pos[2]) << 10 | static_cast<uint32_t>(face) << 15 |
                ao[k] << 18 | sky[k] << 20 | block[k] << 24,
            static_cast<uint32_t>(u) | static_cast<uint32_t>(v) << 5 | layer << 10,
        });
    }
}

// Surface height (in ninths of a block) of the liquid corner of cell
// (x,y,z) toward (dx,dz): the max level over the up-to-4 liquid cells
// sharing that corner, or a full 9 if any of them is submerged (liquid
// above — ocean interiors and falling columns stay flush). Sampling the
// corner across neighbors is what makes the surface slope continuously
// from cell to cell instead of stepping.
int CornerNinths(const PaddedVolume& vol, int x, int y, int z, int dx, int dz) {
    const int cx[4] = {x, x + dx, x, x + dx};
    const int cz[4] = {z, z, z + dz, z + dz};
    int ninths = 0;
    for (int i = 0; i < 4; ++i) {
        const int p = PadIndex(cx[i], y, cz[i]);
        if (!vol.liquid[p]) {
            continue;
        }
        if (vol.liquid[PadIndex(cx[i], y + 1, cz[i])]) {
            return 9;
        }
        ninths = std::max(ninths, static_cast<int>(vol.level[p]));
    }
    return ninths;
}

// Liquid cells emit per cell into the transparent stream — corner heights
// vary with the neighborhood, so quads can't greedy-merge. Cell-top
// corners carry a "drop" (block top minus surface, in ninths) that
// chunk.vert subtracts, giving sources an 8/9-high surface and flows
// their sloped, decaying sheets.
void EmitLiquidCell(ChunkMesh& mesh, const PaddedVolume& vol, int x, int y, int z) {
    const BlockDef& def = BlockRegistry::Get().Def(vol.id[PadIndex(x, y, z)]);
    const int cell[3] = {x, y, z};
    // Opaque liquids (lava) go in the depth-writing, back-face-culled opaque
    // stream; translucent ones (water) in the blended pass. Same packed
    // vertex format and shader either way — only the draw state differs.
    auto& out = def.liquidOpaque ? mesh.vertices : mesh.transparentVertices;

    for (int face = 0; face < 6; ++face) {
        const FaceBasis& fb = kFaces[face];
        int nb[3] = {x, y, z};
        nb[fb.n] += fb.s;
        const int np = PadIndex(nb[0], nb[1], nb[2]);
        if (vol.opaque[np] || vol.liquid[np]) {
            continue;
        }

        const CornerSample corners[4] = {
            SampleCorner(vol, cell, fb, -1, -1),
            SampleCorner(vol, cell, fb, +1, -1),
            SampleCorner(vol, cell, fb, +1, +1),
            SampleCorner(vol, cell, fb, -1, +1),
        };
        const int cu[4] = {0, 1, 1, 0};
        const int cv[4] = {0, 0, 1, 1};
        const int rotate =
            corners[0].ao + corners[2].ao >= corners[1].ao + corners[3].ao ? 0 : 1;
        const int plane = cell[fb.n] + (fb.s > 0 ? 1 : 0);

        for (int j = 0; j < 4; ++j) {
            const int k = (j + rotate) & 3;
            int pos[3];
            pos[fb.n] = plane;
            pos[fb.uAxis] = cell[fb.uAxis] + cu[k];
            pos[fb.vAxis] = cell[fb.vAxis] + cv[k];
            uint32_t drop = 0;
            if (pos[1] == y + 1) { // cell-top corner: the surface sits below it
                drop = static_cast<uint32_t>(
                    9 - CornerNinths(vol, x, y, z, pos[0] == x + 1 ? 1 : -1,
                                     pos[2] == z + 1 ? 1 : -1));
            }
            const int u = fb.swapUv ? cv[k] : cu[k];
            const int v = fb.swapUv ? cu[k] : cv[k];
            out.push_back({
                static_cast<uint32_t>(pos[0]) | static_cast<uint32_t>(pos[1]) << 5 |
                    static_cast<uint32_t>(pos[2]) << 10 | static_cast<uint32_t>(face) << 15 |
                    static_cast<uint32_t>(corners[k].ao) << 18 |
                    static_cast<uint32_t>(corners[k].sky) << 20 |
                    static_cast<uint32_t>(corners[k].block) << 24,
                static_cast<uint32_t>(u) | static_cast<uint32_t>(v) << 5 |
                    static_cast<uint32_t>(def.faceTiles[face]) << 10 | drop << 26,
            });
        }
    }
}

// Plants render as an X: two diagonal corner-to-corner planes, each
// emitted with both windings so backface culling keeps them visible from
// every side. They go into the regular alpha-tested stream. Lighting is
// the cell's own light, full-bright AO, and a +Y normal — a flat sun
// response, no fake side-shading on what is really a 2D sprite.
void EmitCrossCell(ChunkMesh& mesh, const PaddedVolume& vol, int x, int y, int z) {
    const int p = PadIndex(x, y, z);
    const auto layer =
        static_cast<uint32_t>(BlockRegistry::Get().Def(vol.id[p]).faceTiles[0]);
    const auto sky = static_cast<uint32_t>(ChunkLight::Sky(vol.light[p]));
    const auto block = static_cast<uint32_t>(ChunkLight::Block(vol.light[p]));
    const uint32_t shade = 2u << 15 | 3u << 18 | sky << 20 | block << 24;

    // Bottom endpoints of the two diagonals; corners run BL, BR, TR, TL
    // (CCW seen from the +u side) with v up so the sprite stands upright.
    const int ax[2] = {x, x};
    const int az[2] = {z, z + 1};
    const int bx[2] = {x + 1, x + 1};
    const int bz[2] = {z + 1, z};
    constexpr int cu[4] = {0, 1, 1, 0};
    constexpr int cv[4] = {0, 0, 1, 1};

    for (int plane = 0; plane < 2; ++plane) {
        for (int winding = 0; winding < 2; ++winding) {
            for (int j = 0; j < 4; ++j) {
                const int k = winding == 0 ? j : 3 - j; // reverse = back face
                const int px = cu[k] == 0 ? ax[plane] : bx[plane];
                const int pz = cu[k] == 0 ? az[plane] : bz[plane];
                mesh.vertices.push_back({
                    static_cast<uint32_t>(px) | static_cast<uint32_t>(y + cv[k]) << 5 |
                        static_cast<uint32_t>(pz) << 10 | shade,
                    static_cast<uint32_t>(cu[k]) | static_cast<uint32_t>(cv[k]) << 5 |
                        layer << 10,
                });
            }
        }
    }
}

// Emit one model box (BlockDef::ModelBox) into the float model stream.
// Each requested face becomes a quad with arbitrary fractional corners and
// UVs — the packed cubic vertex can't hold these, which is the whole point
// of the second stream. Reuses the cube mesher's FaceBasis so winding (CCW
// from outside, backface-culled) and the v-up texture orientation match
// the rest of the world. Lighting is the cell's own light and full-bright
// AO, like the cross plants — these are small emissive/decorative shapes,
// not surfaces that want neighbor-sampled gradients.
// M28: build one model box with auto-generated UVs — each face samples the
// slice of its tile that the box occupies, in the face's (u,v) basis (v up),
// exactly like the cube mesher. The texture therefore reads as if cut from a
// full block, so a bottom slab's sides show the tile's lower half and stair
// faces stay continuous with surrounding full blocks. All six faces are
// enabled; hidden interior faces are back-face culled and boundary faces are
// dropped against opaque neighbors in EmitModelBox.
ModelBox ShapeBox(const glm::vec3& from, const glm::vec3& to,
                  const std::array<uint16_t, 6>& tiles) {
    ModelBox box;
    box.from = from;
    box.to = to;
    for (int f = 0; f < 6; ++f) {
        const FaceBasis& fb = kFaces[f];
        ModelBox::Face& face = box.faces[f];
        face.on = true;
        face.tile = tiles[static_cast<size_t>(f)];
        face.uv = {from[fb.uAxis], from[fb.vAxis], to[fb.uAxis], to[fb.vAxis]};
    }
    return box;
}

// Slab: a single half box, bottom (meta 0) or top. Returns the box count.
int BuildSlabBoxes(const BlockDef& def, uint8_t meta, ModelBox out[2]) {
    const bool top = facing::SlabIsTop(meta);
    out[0] = ShapeBox(top ? glm::vec3(0, 8, 0) : glm::vec3(0, 0, 0),
                      top ? glm::vec3(16, 16, 16) : glm::vec3(16, 8, 16), def.faceTiles);
    return 1;
}

// Straight stair: a half slab plus a quarter box (the tall back) on the
// FACING side, mirroring vanilla BlockStairs.getCollisionBoxList /
// getCollQuarterBlock. Upside-down (top half) flips both boxes' Y span.
int BuildStairBoxes(const BlockDef& def, uint8_t meta, ModelBox out[2]) {
    const bool top = facing::StairsIsTop(meta);
    out[0] = ShapeBox(top ? glm::vec3(0, 8, 0) : glm::vec3(0, 0, 0),
                      top ? glm::vec3(16, 16, 16) : glm::vec3(16, 8, 16), def.faceTiles);
    glm::vec3 qFrom(0, top ? 0 : 8, 0);
    glm::vec3 qTo(16, top ? 8 : 16, 16);
    switch (facing::StairsFacing(meta)) {
    case BlockFace::PosX: qFrom.x = 8; break; // tall side east
    case BlockFace::NegX: qTo.x = 8; break;   // west
    case BlockFace::PosZ: qFrom.z = 8; break; // south
    case BlockFace::NegZ: qTo.z = 8; break;   // north
    default: break;                           // only horizontals are stored
    }
    out[1] = ShapeBox(qFrom, qTo, def.faceTiles);
    return 2;
}

// `cullBoundary` is set for the axis-aligned slab/stair boxes (identity
// xform): a box face whose plane sits exactly on the cell boundary (0 or 16
// on its normal axis) is dropped when the neighbor that way is opaque — this
// kills the z-fight between a slab's bottom face and the ground it sits on,
// or a stair's back face and the wall behind it. It's a no-op for the torch
// (its planes are inset, never on a boundary) and is skipped for oriented
// (rotated) boxes where "boundary" no longer means axis-aligned.
void EmitModelBox(ChunkMesh& mesh, const PaddedVolume& vol, int cellX, int cellY, int cellZ,
                  const ModelBox& box, uint32_t sky, uint32_t block, const glm::mat4& xform,
                  bool cullBoundary) {
    constexpr float kInv16 = 1.0f / 16.0f;
    const float cell[3] = {static_cast<float>(cellX), static_cast<float>(cellY),
                           static_cast<float>(cellZ)};
    constexpr int cu[4] = {0, 1, 1, 0};
    constexpr int cv[4] = {0, 0, 1, 1};

    for (int face = 0; face < 6; ++face) {
        const ModelBox::Face& bf = box.faces[face];
        if (!bf.on) {
            continue;
        }
        const FaceBasis& fb = kFaces[face];
        if (cullBoundary) {
            const bool onBoundary =
                fb.s > 0 ? box.to[fb.n] >= 16.0f : box.from[fb.n] <= 0.0f;
            if (onBoundary) {
                int np[3] = {cellX, cellY, cellZ};
                np[fb.n] += fb.s;
                if (vol.opaque[PadIndex(np[0], np[1], np[2])]) {
                    continue;
                }
            }
        }
        const float plane = (fb.s > 0 ? box.to[fb.n] : box.from[fb.n]) * kInv16;
        const float uLo = box.from[fb.uAxis] * kInv16;
        const float uHi = box.to[fb.uAxis] * kInv16;
        const float vLo = box.from[fb.vAxis] * kInv16;
        const float vHi = box.to[fb.vAxis] * kInv16;
        // BlockFace order matches the model-stream normal index.
        const uint32_t packed = static_cast<uint32_t>(bf.tile) |
                                static_cast<uint32_t>(face) << 16 | 15u << 19 | sky << 23 |
                                block << 27;
        for (int k = 0; k < 4; ++k) {
            float pos[3];
            pos[fb.n] = plane;
            pos[fb.uAxis] = cu[k] ? uHi : uLo;
            pos[fb.vAxis] = cv[k] ? vHi : vLo;
            // UV from the face's sub-rect; swapUv keeps texture-up aligned
            // with world-up on the faces whose mask axes are transposed.
            const float tu = (cu[k] ? bf.uv.z : bf.uv.x) * kInv16;
            const float tv = (cv[k] ? bf.uv.w : bf.uv.y) * kInv16;
            // Orient the box within the cell (identity for floor torches and
            // unoriented model blocks; a tilt+shift for wall torches). The
            // transform is a rigid motion, so winding/backface culling and
            // the (kept) face-index normal stay valid.
            const glm::vec4 t = xform * glm::vec4(pos[0], pos[1], pos[2], 1.0f);
            mesh.modelVertices.push_back({
                cell[0] + t.x,
                cell[1] + t.y,
                cell[2] + t.z,
                fb.swapUv ? tv : tu,
                fb.swapUv ? tu : tv,
                packed,
            });
        }
    }
}

// A model-block cell emits all of its boxes. Light is the cell's own
// (the BFS seeds emissive blocks like the torch at their emission); AO is
// full-bright. Future model blocks (slabs/stairs) just declare boxes.
// Cell-local orientation transform (block units, around the cell). Identity
// unless the block reads its M24 meta — currently only the wall torch, whose
// upright floor model is tilted out from the wall and shoved against it
// (vanilla's template_torch_wall). The constants are eyeballed to read as a
// mounted torch; tweak the tilt/shift if it sits wrong.
glm::mat4 ModelOrientation(const BlockDef& def, uint8_t meta) {
    if (!(def.torch && facing::TorchIsWall(meta))) {
        return glm::mat4(1.0f);
    }
    const glm::vec3 dir = glm::vec3(facing::Dir(facing::TorchWallFacing(meta))); // points out
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 axis = glm::normalize(glm::cross(up, dir)); // tilt top toward `dir`
    const glm::vec3 pivot(0.5f, 3.5f / 16.0f, 0.5f); // lower attach point (vanilla origin)
    const float tilt = glm::radians(22.5f);          // lean away from the wall
    const glm::vec3 shift = -dir * 0.45f + up * 0.10f; // base against the wall, raised a touch
    return glm::translate(glm::mat4(1.0f), pivot + shift) *
           glm::rotate(glm::mat4(1.0f), tilt, axis) *
           glm::translate(glm::mat4(1.0f), -pivot);
}

void EmitModelCell(ChunkMesh& mesh, const PaddedVolume& vol, int x, int y, int z) {
    const int p = PadIndex(x, y, z);
    const BlockDef& def = BlockRegistry::Get().Def(vol.id[p]);
    const auto sky = static_cast<uint32_t>(ChunkLight::Sky(vol.light[p]));
    const auto block = std::max(static_cast<uint32_t>(ChunkLight::Block(vol.light[p])),
                                static_cast<uint32_t>(def.emission));
    const uint8_t meta = vol.meta[p];

    // M28 slabs/stairs: build their boxes from meta + the base material's
    // faceTiles, identity transform, axis-aligned — so boundary faces cull
    // against opaque neighbors (no z-fight against the ground/wall).
    if (def.slab || def.stairs) {
        ModelBox boxes[2];
        const int n = def.slab ? BuildSlabBoxes(def, meta, boxes)
                               : BuildStairBoxes(def, meta, boxes);
        for (int i = 0; i < n; ++i) {
            EmitModelBox(mesh, vol, x, y, z, boxes[i], sky, block, glm::mat4(1.0f), true);
        }
        return;
    }

    const glm::mat4 xform = ModelOrientation(def, meta);
    for (const ModelBox& box : def.model) {
        EmitModelBox(mesh, vol, x, y, z, box, sky, block, xform, false);
    }
}

// Face connectivity for occlusion culling: flood fill the chunk's
// non-opaque cells; every component connects all the faces it touches.
// Cell index i is YZX (Chunk::Index), so x = i & 15, z = (i>>4) & 15,
// y = i >> 8.
VisibilityBits ComputeVisibility(const PaddedVolume& vol) {
    std::array<uint8_t, Chunk::kVolume> visited{};
    std::array<uint16_t, Chunk::kVolume> queue;
    VisibilityBits bits = 0;

    for (int start = 0; start < Chunk::kVolume; ++start) {
        const int sx = start & 15;
        const int sz = (start >> 4) & 15;
        const int sy = start >> 8;
        if (visited[start] || vol.opaque[PadIndex(sx, sy, sz)]) {
            continue;
        }
        visited[start] = 1;
        queue[0] = static_cast<uint16_t>(start);
        size_t head = 0;
        size_t tail = 1;
        int faces = 0; // touched-face mask, BlockFace order

        while (head < tail) {
            const int i = queue[head++];
            const int x = i & 15;
            const int z = (i >> 4) & 15;
            const int y = i >> 8;
            faces |= (x == 15) << 0 | (x == 0) << 1 | (y == 15) << 2 | (y == 0) << 3 |
                     (z == 15) << 4 | (z == 0) << 5;

            constexpr int kStep[6][4] = {
                // dx, dy, dz, Chunk::Index delta
                {+1, 0, 0, 1},   {-1, 0, 0, -1},   {0, +1, 0, 256},
                {0, -1, 0, -256}, {0, 0, +1, 16},  {0, 0, -1, -16},
            };
            for (const auto& s : kStep) {
                const int nx = x + s[0];
                const int ny = y + s[1];
                const int nz = z + s[2];
                if (!Chunk::InBounds(nx, ny, nz)) {
                    continue;
                }
                const int ni = i + s[3];
                if (visited[ni] || vol.opaque[PadIndex(nx, ny, nz)]) {
                    continue;
                }
                visited[ni] = 1;
                queue[tail++] = static_cast<uint16_t>(ni);
            }
        }

        for (int a = 0; a < 6; ++a) {
            for (int b = a + 1; b < 6; ++b) {
                if ((faces >> a & 1) && (faces >> b & 1)) {
                    bits |= static_cast<VisibilityBits>(1u << FacePairBit(a, b));
                }
            }
        }
        if (bits == kAllFacesConnected) {
            return bits; // can't grow further
        }
    }
    return bits;
}

} // namespace

ChunkMesh ChunkMesher::Build(const ChunkSnapshot& snapshot) {
    VOX_ASSERT(snapshot.chunks[ChunkSnapshot::Index(1, 1, 1)],
               "Meshing a snapshot with no center chunk");

    PaddedVolume vol;
    FillPadded(snapshot, vol);
    const auto& registry = BlockRegistry::Get();

    ChunkMesh mesh;
    mesh.visibility = ComputeVisibility(vol);
    uint64_t mask[Chunk::kSize * Chunk::kSize];

    for (int face = 0; face < 6; ++face) {
        const FaceBasis& fb = kFaces[face];
        for (int slice = 0; slice < Chunk::kSize; ++slice) {
            // Build the visibility mask for this slice. Opaque blocks only
            // — liquids are emitted per cell after the greedy passes.
            for (int v = 0; v < Chunk::kSize; ++v) {
                for (int u = 0; u < Chunk::kSize; ++u) {
                    int cell[3];
                    cell[fb.n] = slice;
                    cell[fb.uAxis] = u;
                    cell[fb.vAxis] = v;
                    const int p = PadIndex(cell[0], cell[1], cell[2]);

                    // Cutout cubes (leaves) mesh exactly like opaque ones
                    // but hide nothing: every face against a non-opaque
                    // neighbor renders, including leaf-on-leaf (vanilla
                    // "fancy"; the coplanar opposite-winding pairs resolve
                    // by backface culling). Alpha test trims the holes.
                    uint64_t key = 0;
                    if (vol.opaque[p] || vol.cutout[p]) {
                        int nb[3] = {cell[0], cell[1], cell[2]};
                        nb[fb.n] += fb.s;
                        if (!vol.opaque[PadIndex(nb[0], nb[1], nb[2])]) {
                            const BlockDef& def = registry.Def(vol.id[p]);
                            CornerSample corners[4] = {
                                SampleCorner(vol, cell, fb, -1, -1),
                                SampleCorner(vol, cell, fb, +1, -1),
                                SampleCorner(vol, cell, fb, +1, +1),
                                SampleCorner(vol, cell, fb, -1, +1),
                            };
                            // Emissive blocks light their own faces even
                            // when neighbor cells read dimmer.
                            if (def.emission > 0) {
                                for (auto& corner : corners) {
                                    corner.block = std::max(corner.block,
                                                            static_cast<int>(def.emission));
                                }
                            }
                            key = MaskKey(OrientedFaceTile(def, vol.meta[p], face), corners);
                        }
                    }
                    mask[v * Chunk::kSize + u] = key;
                }
            }

            // Greedy merge: widest run first, then grow rows downward.
            for (int v = 0; v < Chunk::kSize; ++v) {
                for (int u = 0; u < Chunk::kSize;) {
                    const uint64_t key = mask[v * Chunk::kSize + u];
                    if (key == 0) {
                        ++u;
                        continue;
                    }
                    int w = 1;
                    while (u + w < Chunk::kSize && mask[v * Chunk::kSize + u + w] == key) {
                        ++w;
                    }
                    int h = 1;
                    while (v + h < Chunk::kSize) {
                        bool rowMatches = true;
                        for (int k = 0; k < w; ++k) {
                            if (mask[(v + h) * Chunk::kSize + u + k] != key) {
                                rowMatches = false;
                                break;
                            }
                        }
                        if (!rowMatches) {
                            break;
                        }
                        ++h;
                    }

                    EmitQuad(mesh, face, fb, slice, u, v, w, h, key);

                    for (int dv = 0; dv < h; ++dv) {
                        for (int k = 0; k < w; ++k) {
                            mask[(v + dv) * Chunk::kSize + u + k] = 0;
                        }
                    }
                    u += w;
                }
            }
        }
    }

    // Per-cell passes: liquids (corner-sampled surface heights), plant
    // crosses, and model boxes (torches) — none can greedy-merge.
    for (int y = 0; y < Chunk::kSize; ++y) {
        for (int z = 0; z < Chunk::kSize; ++z) {
            for (int x = 0; x < Chunk::kSize; ++x) {
                const int p = PadIndex(x, y, z);
                if (vol.liquid[p]) {
                    EmitLiquidCell(mesh, vol, x, y, z);
                } else if (vol.cross[p]) {
                    EmitCrossCell(mesh, vol, x, y, z);
                } else if (vol.model[p]) {
                    EmitModelCell(mesh, vol, x, y, z);
                }
            }
        }
    }
    return mesh;
}

} // namespace vc
