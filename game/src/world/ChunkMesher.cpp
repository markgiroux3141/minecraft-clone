#include "world/ChunkMesher.h"

#include <algorithm>
#include <cstdint>

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
    std::array<uint8_t, kPadVolume> opaque;
    std::array<uint8_t, kPadVolume> liquid;
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
                out.opaque[p] = def.opaque ? 1 : 0;
                out.liquid[p] = def.liquid ? 1 : 0;

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

// Mask cell key (uint64): bit 57 = transparent stream, bit 56 = face
// present, bits 40..55 = corner block light (4x4), bits 24..39 = corner
// sky light (4x4), bits 8..23 = texture layer, bits 0..7 = corner AO
// (4x2). Corners are stored in emitted order. Cells merge only on exact
// key match, which keeps both AO and light gradients continuous across
// merged quads (and never merges liquid with solid faces).
constexpr uint64_t kKeyTransparent = uint64_t{1} << 57;

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
    auto& vertices = (key & kKeyTransparent) ? mesh.transparentVertices : mesh.vertices;
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
            // Build the visibility mask for this slice. Opaque faces show
            // against any non-opaque neighbor; liquid faces only against
            // non-opaque non-liquid neighbors (no internal water-water
            // faces) and go to the transparent stream.
            for (int v = 0; v < Chunk::kSize; ++v) {
                for (int u = 0; u < Chunk::kSize; ++u) {
                    int cell[3];
                    cell[fb.n] = slice;
                    cell[fb.uAxis] = u;
                    cell[fb.vAxis] = v;
                    const int p = PadIndex(cell[0], cell[1], cell[2]);

                    uint64_t key = 0;
                    if (vol.opaque[p] || vol.liquid[p]) {
                        int nb[3] = {cell[0], cell[1], cell[2]};
                        nb[fb.n] += fb.s;
                        const int np = PadIndex(nb[0], nb[1], nb[2]);
                        const bool visible = vol.opaque[p]
                                                 ? !vol.opaque[np]
                                                 : !vol.opaque[np] && !vol.liquid[np];
                        if (visible) {
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
                            key = MaskKey(def.faceTiles[face], corners);
                            if (vol.liquid[p]) {
                                key |= kKeyTransparent;
                            }
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
    return mesh;
}

} // namespace vc
