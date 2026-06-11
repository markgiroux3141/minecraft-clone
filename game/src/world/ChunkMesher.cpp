#include "world/ChunkMesher.h"

#include <cstdint>

#include "vox/core/Assert.h"

namespace vc {

namespace {

// The chunk plus a one-block shell of its neighbors, flattened for O(1)
// lookups during meshing. ~18 KB, lives on the worker's stack.
constexpr int kPad = Chunk::kSize + 2;
constexpr int kPadVolume = kPad * kPad * kPad;

constexpr int PadIndex(int x, int y, int z) {
    return ((y + 1) * kPad + (z + 1)) * kPad + (x + 1);
}

struct PaddedVolume {
    std::array<BlockId, kPadVolume> id;
    std::array<uint8_t, kPadVolume> opaque;
};

void FillPadded(const ChunkSnapshot& snapshot, PaddedVolume& out) {
    const auto& registry = BlockRegistry::Get();
    for (int y = -1; y <= Chunk::kSize; ++y) {
        for (int z = -1; z <= Chunk::kSize; ++z) {
            for (int x = -1; x <= Chunk::kSize; ++x) {
                // Arithmetic shift maps -1 -> slot 0, 0..15 -> 1, 16 -> 2.
                const int slot =
                    ChunkSnapshot::Index((x >> 4) + 1, (y >> 4) + 1, (z >> 4) + 1);
                const Chunk* chunk = snapshot.chunks[slot].get();
                const BlockId id = chunk ? chunk->Get(x & 15, y & 15, z & 15) : blocks::Air;
                const int p = PadIndex(x, y, z);
                out.id[p] = id;
                out.opaque[p] = registry.Def(id).opaque ? 1 : 0;
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

// Classic corner AO: the three blocks touching this vertex in the plane
// one step out along the face normal. 0 = darkest, 3 = open.
int CornerAo(const PaddedVolume& vol, const int cell[3], const FaceBasis& fb, int offU,
             int offV) {
    int above[3] = {cell[0], cell[1], cell[2]};
    above[fb.n] += fb.s;
    const auto opaqueAt = [&](int du, int dv) {
        int p[3] = {above[0], above[1], above[2]};
        p[fb.uAxis] += du;
        p[fb.vAxis] += dv;
        return vol.opaque[PadIndex(p[0], p[1], p[2])] != 0;
    };
    const bool side1 = opaqueAt(offU, 0);
    const bool side2 = opaqueAt(0, offV);
    if (side1 && side2) {
        return 0;
    }
    const bool corner = opaqueAt(offU, offV);
    return 3 - (static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner));
}

// Mask cell key: bit 31 = face present, bits 8..23 = texture layer,
// bits 0..7 = the four corner AO values (2 bits each, in emitted corner
// order). Cells merge only on exact key match, which keeps AO gradients
// continuous across merged quads.
uint32_t MaskKey(uint32_t layer, int ao00, int ao10, int ao11, int ao01) {
    return 0x80000000u | (layer << 8) | static_cast<uint32_t>(ao00 << 6) |
           static_cast<uint32_t>(ao10 << 4) | static_cast<uint32_t>(ao11 << 2) |
           static_cast<uint32_t>(ao01);
}

void EmitQuad(ChunkMesh& mesh, const FaceBasis& fb, int slice, int u0, int v0, int w, int h,
              uint32_t key) {
    const int plane = slice + (fb.s > 0 ? 1 : 0);
    const auto layer = static_cast<float>((key >> 8) & 0xFFFFu);
    const int ao[4] = {
        static_cast<int>(key >> 6) & 3,
        static_cast<int>(key >> 4) & 3,
        static_cast<int>(key >> 2) & 3,
        static_cast<int>(key) & 3,
    };
    const int cu[4] = {0, w, w, 0};
    const int cv[4] = {0, 0, h, h};

    glm::vec3 normal(0.0f);
    normal[fb.n] = static_cast<float>(fb.s);

    const auto base = static_cast<uint32_t>(mesh.vertices.size());
    for (int k = 0; k < 4; ++k) {
        glm::vec3 pos;
        pos[fb.n] = static_cast<float>(plane);
        pos[fb.uAxis] = static_cast<float>(u0 + cu[k]);
        pos[fb.vAxis] = static_cast<float>(v0 + cv[k]);
        const glm::vec2 uv = fb.swapUv
                                 ? glm::vec2{static_cast<float>(cv[k]), static_cast<float>(cu[k])}
                                 : glm::vec2{static_cast<float>(cu[k]), static_cast<float>(cv[k])};
        mesh.vertices.push_back({
            pos,
            normal,
            uv,
            layer,
            static_cast<float>(ao[k]) / 3.0f,
        });
    }
    // Split along the brighter diagonal so AO interpolates without the
    // dark-cross anisotropy artifact.
    if (ao[0] + ao[2] >= ao[1] + ao[3]) {
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 2, base + 2, base + 3, base});
    } else {
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 3, base + 1, base + 2, base + 3});
    }
}

} // namespace

ChunkMesh ChunkMesher::Build(const ChunkSnapshot& snapshot) {
    VOX_ASSERT(snapshot.chunks[ChunkSnapshot::Index(1, 1, 1)],
               "Meshing a snapshot with no center chunk");

    PaddedVolume vol;
    FillPadded(snapshot, vol);
    const auto& registry = BlockRegistry::Get();

    ChunkMesh mesh;
    uint32_t mask[Chunk::kSize * Chunk::kSize];

    for (int face = 0; face < 6; ++face) {
        const FaceBasis& fb = kFaces[face];
        for (int slice = 0; slice < Chunk::kSize; ++slice) {
            // Build the visibility mask for this slice. Only opaque blocks
            // mesh for now; transparent block types (water, leaves) will
            // need their own pass when they exist.
            for (int v = 0; v < Chunk::kSize; ++v) {
                for (int u = 0; u < Chunk::kSize; ++u) {
                    int cell[3];
                    cell[fb.n] = slice;
                    cell[fb.uAxis] = u;
                    cell[fb.vAxis] = v;
                    const int p = PadIndex(cell[0], cell[1], cell[2]);

                    uint32_t key = 0;
                    if (vol.opaque[p]) {
                        int nb[3] = {cell[0], cell[1], cell[2]};
                        nb[fb.n] += fb.s;
                        if (!vol.opaque[PadIndex(nb[0], nb[1], nb[2])]) {
                            const uint32_t layer = registry.Def(vol.id[p]).faceTiles[face];
                            key = MaskKey(layer,
                                          CornerAo(vol, cell, fb, -1, -1),
                                          CornerAo(vol, cell, fb, +1, -1),
                                          CornerAo(vol, cell, fb, +1, +1),
                                          CornerAo(vol, cell, fb, -1, +1));
                        }
                    }
                    mask[v * Chunk::kSize + u] = key;
                }
            }

            // Greedy merge: widest run first, then grow rows downward.
            for (int v = 0; v < Chunk::kSize; ++v) {
                for (int u = 0; u < Chunk::kSize;) {
                    const uint32_t key = mask[v * Chunk::kSize + u];
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

                    EmitQuad(mesh, fb, slice, u, v, w, h, key);

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
