#include "world/TerrainGen.h"

#include <algorithm>

#include <FastNoiseLite.h>

namespace vc {

void TerrainGenerator::Generate(Chunk& chunk, const glm::ivec3& chunkCoord) const {
    FastNoiseLite noise(m_seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(4);
    noise.SetFrequency(0.008f);

    const glm::ivec3 origin = chunkCoord * Chunk::kSize;

    for (int z = 0; z < Chunk::kSize; ++z) {
        for (int x = 0; x < Chunk::kSize; ++x) {
            const float n = noise.GetNoise(static_cast<float>(origin.x + x),
                                           static_cast<float>(origin.z + z)); // [-1, 1]
            const int height = 24 + static_cast<int>(20.0f * n);

            for (int y = 0; y < Chunk::kSize; ++y) {
                const int wy = origin.y + y;
                if (wy > height) {
                    break; // air above the surface; chunk is air-initialized
                }
                BlockId id = blocks::Stone;
                if (wy == height) {
                    id = blocks::Grass;
                } else if (wy >= height - 3) {
                    id = blocks::Dirt;
                }
                chunk.Set(x, y, z, id);
            }
        }
    }
}

} // namespace vc
