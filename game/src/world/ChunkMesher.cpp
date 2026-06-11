#include "world/ChunkMesher.h"

#include "vox/core/Assert.h"

namespace vc {

namespace {

// Indexed by BlockFace. Corners wind CCW viewed from outside the block, in
// the order matching UVs (0,0) (1,0) (1,1) (0,1).
constexpr glm::ivec3 kFaceNormals[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

constexpr glm::ivec3 kFaceCorners[6][4] = {
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, // +X
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, // -X
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, // +Y
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, // -Y
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, // +Z
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, // -Z
};

constexpr glm::vec2 kFaceUvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

BlockId BlockAt(const ChunkSnapshot& snapshot, int x, int y, int z) {
    if (Chunk::InBounds(x, y, z)) {
        return snapshot.center->Get(x, y, z);
    }
    // Queries are always face-adjacent, so exactly one axis is out of
    // bounds by one — pick that neighbor and wrap the coordinate.
    int face;
    if (x >= Chunk::kSize) {
        face = 0;
    } else if (x < 0) {
        face = 1;
    } else if (y >= Chunk::kSize) {
        face = 2;
    } else if (y < 0) {
        face = 3;
    } else if (z >= Chunk::kSize) {
        face = 4;
    } else {
        face = 5;
    }
    const Chunk* neighbor = snapshot.neighbors[face].get();
    if (!neighbor) {
        return blocks::Air;
    }
    return neighbor->Get(x & 15, y & 15, z & 15);
}

bool IsOpaqueAt(const ChunkSnapshot& snapshot, int x, int y, int z) {
    return BlockRegistry::Get().Def(BlockAt(snapshot, x, y, z)).opaque;
}

} // namespace

ChunkMesh ChunkMesher::Build(const ChunkSnapshot& snapshot) {
    VOX_ASSERT(snapshot.center, "Meshing a snapshot with no center chunk");
    const Chunk& chunk = *snapshot.center;

    ChunkMesh mesh;
    const auto& registry = BlockRegistry::Get();

    for (int y = 0; y < Chunk::kSize; ++y) {
        for (int z = 0; z < Chunk::kSize; ++z) {
            for (int x = 0; x < Chunk::kSize; ++x) {
                const BlockId id = chunk.Get(x, y, z);
                if (id == blocks::Air) {
                    continue;
                }
                const BlockDef& def = registry.Def(id);

                for (int face = 0; face < 6; ++face) {
                    const glm::ivec3 n = kFaceNormals[face];
                    if (IsOpaqueAt(snapshot, x + n.x, y + n.y, z + n.z)) {
                        continue;
                    }

                    const auto base = static_cast<uint32_t>(mesh.vertices.size());
                    const auto layer = static_cast<float>(def.faceTiles[face]);
                    for (int corner = 0; corner < 4; ++corner) {
                        mesh.vertices.push_back({
                            glm::vec3(glm::ivec3{x, y, z} + kFaceCorners[face][corner]),
                            glm::vec3(n),
                            kFaceUvs[corner],
                            layer,
                        });
                    }
                    mesh.indices.insert(mesh.indices.end(),
                                        {base, base + 1, base + 2, base + 2, base + 3, base});
                }
            }
        }
    }
    return mesh;
}

} // namespace vc
