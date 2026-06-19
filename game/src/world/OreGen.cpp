#include "world/OreGen.h"

#include <cmath>
#include <numbers>
#include <utility>

#include "world/Block.h"
#include "world/JavaRandom.h"

namespace vc::ores {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Vein parameters, vanilla ChunkGeneratorSettings (coal 20 veins/size 17/
// y0..128, iron 20/9/y0..64). M25 made the world 128 tall with the surface
// near vanilla's y64, so these are now essentially the vanilla values:
// coal spans the whole underground, iron fills the lower half.
struct OreParams {
    int veins;
    int size;
    int minY, rangeY;
};
constexpr OreParams kCoal{20, 17, 2, 123};  // y 2..124 (whole underground)
constexpr OreParams kIron{20, 9, 2, 61};    // y 2..62 (lower half, vanilla-ish)
constexpr OreParams kRedstone{8, 8, 2, 14}; // y 2..15 (deep, vanilla 8/8/0..16)

// WorldGenMinable.generate, verbatim: an ellipsoid of shrinking-then-
// growing radius swept along a random diagonal through (sx+8, sy, sz+8).
// All random draws happen unconditionally so the sequence stays identical
// no matter which target chunk is replaying this vein.
void GenerateVein(Chunk& chunk, const glm::ivec3& origin, JavaRandom& rand, int sx, int sy,
                  int sz, int size, BlockId ore) {
    const float f = rand.NextFloat() * kPi;
    const auto fSize = static_cast<float>(size);
    const double d0 = (static_cast<float>(sx + 8) + std::sin(f) * fSize / 8.0f);
    const double d1 = (static_cast<float>(sx + 8) - std::sin(f) * fSize / 8.0f);
    const double d2 = (static_cast<float>(sz + 8) + std::cos(f) * fSize / 8.0f);
    const double d3 = (static_cast<float>(sz + 8) - std::cos(f) * fSize / 8.0f);
    const double d4 = sy + rand.NextInt(3) - 2;
    const double d5 = sy + rand.NextInt(3) - 2;

    for (int i = 0; i < size; ++i) {
        const float f1 = static_cast<float>(i) / fSize;
        const double d6 = d0 + (d1 - d0) * f1;
        const double d7 = d4 + (d5 - d4) * f1;
        const double d8 = d2 + (d3 - d2) * f1;
        const double d9 = rand.NextDouble() * size / 16.0;
        const double d10 = (std::sin(kPi * f1) + 1.0f) * d9 + 1.0;

        const int x0 = static_cast<int>(std::floor(d6 - d10 / 2.0));
        const int y0 = static_cast<int>(std::floor(d7 - d10 / 2.0));
        const int z0 = static_cast<int>(std::floor(d8 - d10 / 2.0));
        const int x1 = static_cast<int>(std::floor(d6 + d10 / 2.0));
        const int y1 = static_cast<int>(std::floor(d7 + d10 / 2.0));
        const int z1 = static_cast<int>(std::floor(d8 + d10 / 2.0));

        for (int x = x0; x <= x1; ++x) {
            const double dx = (x + 0.5 - d6) / (d10 / 2.0);
            if (dx * dx >= 1.0) {
                continue;
            }
            for (int y = y0; y <= y1; ++y) {
                const double dy = (y + 0.5 - d7) / (d10 / 2.0);
                if (dx * dx + dy * dy >= 1.0) {
                    continue;
                }
                for (int z = z0; z <= z1; ++z) {
                    const double dz = (z + 0.5 - d8) / (d10 / 2.0);
                    if (dx * dx + dy * dy + dz * dz >= 1.0) {
                        continue;
                    }
                    const int lx = x - origin.x;
                    const int ly = y - origin.y;
                    const int lz = z - origin.z;
                    if (lx < 0 || lx >= Chunk::kSize || ly < 0 || ly >= Chunk::kSize ||
                        lz < 0 || lz >= Chunk::kSize) {
                        continue;
                    }
                    if (chunk.Get(lx, ly, lz) == blocks::Stone) {
                        chunk.Set(lx, ly, lz, ore);
                    }
                }
            }
        }
    }
}

} // namespace

void Place(Chunk& chunk, const glm::ivec3& chunkCoord, int seed) {
    const glm::ivec3 origin = chunkCoord * Chunk::kSize;

    // Replay every origin chunk whose veins can reach this one. Origins
    // are visited in a fixed (ascending) order and each draws its full
    // random sequence, so overlapping veins resolve identically from
    // every chunk that sees them.
    for (int ocz = chunkCoord.z - 1; ocz <= chunkCoord.z + 1; ++ocz) {
        for (int ocx = chunkCoord.x - 1; ocx <= chunkCoord.x + 1; ++ocx) {
            // Vanilla's population-seed mix (chunkX*a + chunkZ*b ^ seed).
            JavaRandom rand((static_cast<int64_t>(ocx) * 341873128712LL +
                             static_cast<int64_t>(ocz) * 132897987541LL) ^
                            static_cast<int64_t>(seed));
            // Redstone draws come AFTER coal+iron, so their vein sequences are
            // unchanged (only deep redstone is added to pre-RS1 seeds).
            for (const auto& [params, ore] :
                 {std::pair{kCoal, blocks::CoalOre}, std::pair{kIron, blocks::IronOre},
                  std::pair{kRedstone, blocks::RedstoneOre}}) {
                for (int v = 0; v < params.veins; ++v) {
                    const int sx = ocx * Chunk::kSize + rand.NextInt(16);
                    const int sy = params.minY + rand.NextInt(params.rangeY);
                    const int sz = ocz * Chunk::kSize + rand.NextInt(16);
                    GenerateVein(chunk, origin, rand, sx, sy, sz, params.size, ore);
                }
            }
        }
    }
}

} // namespace vc::ores
