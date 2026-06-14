#include "world/TerrainGen.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

#include <FastNoiseLite.h>

#include "world/CaveGen.h"
#include "world/LakeGen.h"
#include "world/Light.h" // kWorldHeightBlocks
#include "world/OreGen.h"

namespace vc {

namespace {

// Tree placement: the world is tiled into kTreeCell^2-column cells; each
// cell hash-decides whether it hosts a tree and where inside the cell it
// stands. Cell-based placement keeps trees naturally spaced and lets a
// chunk enumerate exactly the cells whose canopy could reach it.
constexpr int kTreeCell = 8;
constexpr int kCanopyRadius = 2;
// Beach band: columns this close to sea level get sand and no trees.
constexpr int kBeachTop = TerrainGenerator::kSeaLevel + 2;

// M15 biomes: two very-low-frequency climate fields (temperature and
// moisture, ~300-block features) classify each column. Tree density and
// the surface blocks follow the biome; the beach band overrides near sea
// level.
// M27 oceans: a third even-lower-frequency "continentalness" field overrides
// the climate biome where it dips low — Ocean / DeepOcean carry vanilla's
// NEGATIVE baseHeight (Biome.java: ocean -1.0, deep ocean -1.8), which the
// height blend turns into basins well below sea level for the M11 water fill.
enum class Biome : uint8_t { Plains, Forest, Desert, Snowy, Ocean, DeepOcean };

// Continentalness thresholds on the raw [-1,1] field: below kOceanLevel is
// ocean, below kDeepOceanLevel is deep ocean. The values sit on the negative
// tail so land still dominates the map; the param blend (cellParams 5x5)
// smooths base height across the threshold into a sloping coastline. Tuned
// (measured on the seed-1337 field) to ~25% total ocean / ~13% deep ocean —
// plenty of water but land-dominant so there's room to build.
constexpr float kOceanLevel = -0.50f;
constexpr float kDeepOceanLevel = -0.72f;

Biome ClassifyBiome(float temperature, float moisture) {
    if (temperature > 0.65f && moisture < 0.40f) {
        return Biome::Desert;
    }
    if (temperature < 0.32f) {
        return Biome::Snowy;
    }
    if (moisture > 0.55f) {
        return Biome::Forest;
    }
    return Biome::Plains;
}

float TreeChance(Biome biome) {
    switch (biome) {
    case Biome::Forest: return 0.85f;
    case Biome::Plains: return 0.18f;
    case Biome::Snowy: return 0.08f;
    case Biome::Desert: return 0.0f;
    case Biome::Ocean: return 0.0f;
    case Biome::DeepOcean: return 0.0f;
    }
    return 0.0f;
}

// Topology, vanilla-style (ChunkGeneratorOverworld.generateHeightmap): each
// biome contributes (baseHeight, heightVariation) in 1.12's units — desert/
// plains 0.125/0.05 (flat), forest 0.1/0.2, taiga 0.2/0.2 for snowy. A
// ruggedness field ramps cells toward the hills variants (extreme hills
// 1.0/0.5; desert hills 0.45/0.3). Heights blend the params over a 5x5
// neighborhood of 4-block biome cells with vanilla's parabolic kernel.
struct BiomeParams {
    float base;
    float variation;
};

BiomeParams ParamsFor(Biome biome) {
    switch (biome) {
    case Biome::Desert: return {0.125f, 0.05f};
    case Biome::Plains: return {0.125f, 0.05f};
    case Biome::Forest: return {0.1f, 0.2f};
    case Biome::Snowy: return {0.2f, 0.2f};
    case Biome::Ocean: return {-1.0f, 0.1f};      // vanilla Biome.java:473
    case Biome::DeepOcean: return {-1.8f, 0.1f};  // vanilla Biome.java:497
    }
    return {0.1f, 0.2f};
}

constexpr int kBiomeCell = 4; // blend resolution, like vanilla
// Peaks above this get a snow cap regardless of climate (sandy excluded).
// M25: raised with the world (tall hills now reach ~y95+).
constexpr int kSnowLine = 90;

// 10 / sqrt(dx^2 + dz^2 + 0.2), vanilla's biomeWeights.
const std::array<float, 25>& BiomeWeights() {
    static const std::array<float, 25> weights = [] {
        std::array<float, 25> w{};
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                w[static_cast<size_t>((dz + 2) * 5 + dx + 2)] =
                    10.0f / std::sqrt(static_cast<float>(dx * dx + dz * dz) + 0.2f);
            }
        }
        return w;
    }();
    return weights;
}

uint32_t Mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

uint32_t Hash(int seed, int x, int z, int salt) {
    return Mix(static_cast<uint32_t>(seed) ^ Mix(static_cast<uint32_t>(x) * 0x9E3779B9u ^
                                                 static_cast<uint32_t>(z) * 0x85EBCA6Bu ^
                                                 static_cast<uint32_t>(salt) * 0xC2B2AE35u));
}

float Hash01(int seed, int x, int z, int salt) {
    return static_cast<float>(Hash(seed, x, z, salt) >> 8) / 16777216.0f;
}

int FloorDiv(int a, int b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// Writes a world-space block into this chunk if the cell is inside it AND
// still air — decoration never carves terrain or other trees' blocks.
void SetIfAir(Chunk& chunk, const glm::ivec3& origin, int wx, int wy, int wz, BlockId id) {
    const int lx = wx - origin.x;
    const int ly = wy - origin.y;
    const int lz = wz - origin.z;
    if (lx < 0 || lx >= Chunk::kSize || ly < 0 || ly >= Chunk::kSize || lz < 0 ||
        lz >= Chunk::kSize) {
        return;
    }
    if (chunk.Get(lx, ly, lz) == blocks::Air) {
        chunk.Set(lx, ly, lz, id);
    }
}

// M16 species: oak and birch share the classic shape; spruce is conical.
enum class TreeSpecies : uint8_t { Oak, Birch, Spruce };

bool IsAnyLeaves(BlockId id) {
    return id == blocks::Leaves || id == blocks::BirchLeaves || id == blocks::SpruceLeaves;
}

// One square canopy layer of `radius` around (tx, wy, tz), optionally
// clipping the four corners.
void PlaceCanopyLayer(Chunk& chunk, const glm::ivec3& origin, int tx, int wy, int tz,
                      int radius, bool clipCorners, BlockId leavesId) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (clipCorners && std::abs(dx) == radius && std::abs(dz) == radius) {
                continue;
            }
            SetIfAir(chunk, origin, tx + dx, wy, tz + dz, leavesId);
        }
    }
}

// Oak/birch: trunk, two radius-2 leaf layers (minus corners) around the
// trunk top, a 3x3 above, and a plus-shaped cap. Spruce: a cone of
// alternating radius-2/radius-1 layers with a single-leaf tip. Both stay
// within kCanopyRadius and top+2 (the cell-enumeration and world-height
// guards rely on it).
void PlaceTree(Chunk& chunk, const glm::ivec3& origin, int tx, int ground, int tz, int trunk,
               TreeSpecies species) {
    const BlockId logId = species == TreeSpecies::Birch    ? blocks::BirchLog
                          : species == TreeSpecies::Spruce ? blocks::SpruceLog
                                                           : blocks::Log;
    const BlockId leavesId = species == TreeSpecies::Birch    ? blocks::BirchLeaves
                             : species == TreeSpecies::Spruce ? blocks::SpruceLeaves
                                                              : blocks::Leaves;

    // The dirt patch under the trunk (replaces the grass/snowy surface).
    const int lx = tx - origin.x;
    const int ly = ground - origin.y;
    const int lz = tz - origin.z;
    if (lx >= 0 && lx < Chunk::kSize && ly >= 0 && ly < Chunk::kSize && lz >= 0 &&
        lz < Chunk::kSize &&
        (chunk.Get(lx, ly, lz) == blocks::Grass || chunk.Get(lx, ly, lz) == blocks::SnowyGrass)) {
        chunk.Set(lx, ly, lz, blocks::Dirt);
    }

    const int top = ground + trunk;
    for (int wy = ground + 1; wy <= top; ++wy) {
        // Trunks grow through neighboring canopies (dense forests put
        // leaves in the way before the trunk's own cell is visited).
        const int wlx = tx - origin.x;
        const int wly = wy - origin.y;
        const int wlz = tz - origin.z;
        if (wlx >= 0 && wlx < Chunk::kSize && wly >= 0 && wly < Chunk::kSize && wlz >= 0 &&
            wlz < Chunk::kSize &&
            (chunk.Get(wlx, wly, wlz) == blocks::Air ||
             IsAnyLeaves(chunk.Get(wlx, wly, wlz)))) {
            chunk.Set(wlx, wly, wlz, logId);
        }
    }

    if (species == TreeSpecies::Spruce) {
        SetIfAir(chunk, origin, tx, top + 1, tz, leavesId);
        PlaceCanopyLayer(chunk, origin, tx, top, tz, 1, true, leavesId);
        PlaceCanopyLayer(chunk, origin, tx, top - 1, tz, 1, false, leavesId);
        PlaceCanopyLayer(chunk, origin, tx, top - 2, tz, 2, true, leavesId);
        PlaceCanopyLayer(chunk, origin, tx, top - 3, tz, 1, false, leavesId);
        PlaceCanopyLayer(chunk, origin, tx, top - 4, tz, 2, true, leavesId);
        return;
    }

    PlaceCanopyLayer(chunk, origin, tx, top - 1, tz, kCanopyRadius, true, leavesId);
    PlaceCanopyLayer(chunk, origin, tx, top, tz, kCanopyRadius, true, leavesId);
    PlaceCanopyLayer(chunk, origin, tx, top + 1, tz, 1, false, leavesId);
    SetIfAir(chunk, origin, tx - 1, top + 2, tz, leavesId);
    SetIfAir(chunk, origin, tx + 1, top + 2, tz, leavesId);
    SetIfAir(chunk, origin, tx, top + 2, tz - 1, leavesId);
    SetIfAir(chunk, origin, tx, top + 2, tz + 1, leavesId);
    SetIfAir(chunk, origin, tx, top + 2, tz, leavesId);
}

} // namespace

void TerrainGenerator::Generate(Chunk& chunk, const glm::ivec3& chunkCoord) const {
    FastNoiseLite noise(m_seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(4);
    noise.SetFrequency(0.008f);

    // Climate fields: smooth, unfractal, far lower frequency than terrain.
    FastNoiseLite temperature(m_seed + 101);
    temperature.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    temperature.SetFrequency(0.0030f);
    FastNoiseLite moisture(m_seed + 202);
    moisture.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    moisture.SetFrequency(0.0030f);
    // Ruggedness ramps any biome toward its hills variant where it peaks.
    FastNoiseLite rugged(m_seed + 303);
    rugged.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    rugged.SetFrequency(0.0035f);
    // M27 continentalness: lower frequency than climate (~660-block features)
    // so oceans are large, coherent bodies, not noise speckle.
    FastNoiseLite continental(m_seed + 404);
    continental.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    continental.SetFrequency(0.0015f);

    const auto biomeAt = [&](int wx, int wz) {
        const auto fx = static_cast<float>(wx);
        const auto fz = static_cast<float>(wz);
        const float c = continental.GetNoise(fx, fz); // raw [-1,1]
        if (c < kDeepOceanLevel) {
            return Biome::DeepOcean;
        }
        if (c < kOceanLevel) {
            return Biome::Ocean;
        }
        return ClassifyBiome(temperature.GetNoise(fx, fz) * 0.5f + 0.5f,
                             moisture.GetNoise(fx, fz) * 0.5f + 0.5f);
    };

    // Height params per 4-block biome cell, memoized — heightAt blends 25
    // cells per column and columns repeat across trees/caves.
    std::unordered_map<uint64_t, BiomeParams> cellCache;
    const auto cellParams = [&](int cellX, int cellZ) {
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(cellX)) << 32) |
                             static_cast<uint32_t>(cellZ);
        if (const auto it = cellCache.find(key); it != cellCache.end()) {
            return it->second;
        }
        const auto fx = static_cast<float>(cellX * kBiomeCell + 2);
        const auto fz = static_cast<float>(cellZ * kBiomeCell + 2);
        const Biome biome = biomeAt(cellX * kBiomeCell + 2, cellZ * kBiomeCell + 2);
        BiomeParams params = ParamsFor(biome);
        const float r = rugged.GetNoise(fx, fz) * 0.5f + 0.5f;
        const float hills = glm::smoothstep(0.60f, 0.85f, r);
        const float hillBase = biome == Biome::Desert ? 0.45f : 1.0f;
        const float hillVariation = biome == Biome::Desert ? 0.3f : 0.5f;
        params.base = glm::mix(params.base, hillBase, hills);
        params.variation = glm::mix(params.variation, hillVariation, hills);
        cellCache.emplace(key, params);
        return params;
    };

    // Vanilla's height blend: parabolic 5x5 kernel over biome cells, each
    // weight divided by (base + 2) and HALVED when the neighbor is higher
    // than the center cell — that asymmetry keeps cliff bases sharp.
    const auto heightAt = [&](int wx, int wz) {
        const int cellX = FloorDiv(wx, kBiomeCell);
        const int cellZ = FloorDiv(wz, kBiomeCell);
        const BiomeParams center = cellParams(cellX, cellZ);
        const auto& weights = BiomeWeights();
        float base = 0.0f;
        float variation = 0.0f;
        float total = 0.0f;
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                const BiomeParams p = cellParams(cellX + dx, cellZ + dz);
                float w = weights[static_cast<size_t>((dz + 2) * 5 + dx + 2)] / (p.base + 2.0f);
                if (p.base > center.base) {
                    w *= 0.5f;
                }
                base += p.base * w;
                variation += p.variation * w;
                total += w;
            }
        }
        base /= total;
        variation /= total;
        const float n = noise.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
        // M25 rebase to vanilla proportions in the 128-tall world. The +3
        // lift keeps flat plains (~y68) clearly above the beach band (sea+2 =
        // 65) so inland flats are grass, not sand — only columns that dip to
        // the waterline read as beach. base*18 raises hillier biomes; the
        // noise amplitude grows with heightVariation. Extreme hills top out
        // near y102, below the ~y104 spawn so a fresh world never drops the
        // player inside a peak.
        return kSeaLevel + 3 +
               static_cast<int>(base * 18.0f + n * (3.0f + variation * 30.0f));
    };

    const glm::ivec3 origin = chunkCoord * Chunk::kSize;

    for (int z = 0; z < Chunk::kSize; ++z) {
        for (int x = 0; x < Chunk::kSize; ++x) {
            const int height = heightAt(origin.x + x, origin.z + z);
            const Biome biome = biomeAt(origin.x + x, origin.z + z);
            // Beach band overrides the biome near sea level (deserts are
            // already sand; snowy shores stay sand too — close enough).
            const bool sandy = height <= kBeachTop || biome == Biome::Desert;
            // Vanilla (Biome.generateBiomeTerrain): when the sand filler
            // runs out it switches to a nextInt(4)-deep sandstone band, so
            // caves under sandy surfaces mostly open onto sandstone, not
            // hovering sand. Falling is update-driven (BlockFalling only
            // reacts to neighbor changes), so worldgen sand over a carved
            // void would float exactly like vanilla's — this keeps it rare.
            const int sandstoneDepth =
                sandy ? static_cast<int>(Hash(m_seed, origin.x + x, origin.z + z, 9) % 4) : 0;

            for (int y = 0; y < Chunk::kSize; ++y) {
                const int wy = origin.y + y;
                if (wy > height) {
                    if (wy > kSeaLevel) {
                        break; // air above surface and sea; chunk is air-initialized
                    }
                    chunk.Set(x, y, z, blocks::Water);
                    continue;
                }
                // Bedrock floor first (vanilla: bedrock wherever
                // y <= rand.nextInt(5) — solid at 0, raggedly thinning
                // through 4). Unbreakable, and not in the carver's
                // replaceable set, so neither players nor caves open the
                // void below the world.
                if (wy <= 4 &&
                    wy <= static_cast<int>(
                              Hash(m_seed, origin.x + x, origin.z + z, 10 + wy) % 5)) {
                    chunk.Set(x, y, z, blocks::Bedrock);
                    continue;
                }
                BlockId id = blocks::Stone;
                if (sandy) {
                    if (wy >= height - 2) {
                        id = blocks::Sand;
                    } else if (wy >= height - 2 - sandstoneDepth) {
                        id = blocks::Sandstone;
                    }
                } else if (wy == height) {
                    // Snowy climate, or an alpine cap on high hill peaks.
                    id = (biome == Biome::Snowy || height >= kSnowLine) ? blocks::SnowyGrass
                                                                        : blocks::Grass;
                } else if (wy >= height - 3) {
                    id = blocks::Dirt;
                }
                chunk.Set(x, y, z, id);
            }
        }
    }

    // Caves carve between fill and decoration (vanilla order). Same
    // chunk-pure contract as the trees below: every chunk independently
    // re-derives the tunnels that reach it. The mask records carved cells
    // (chunk + skirt) so the tree gate below can veto candidates whose
    // ground was opened — identically from every chunk sharing the tree.
    auto mask = std::make_unique<caves::CarveMask>();
    caves::Carve(chunk, chunkCoord, m_seed, heightAt, mask.get());

    // Ore veins after the carve (carved cells are air, so veins never
    // deposit into open caves — vanilla's generation/population order).
    ores::Place(chunk, chunkCoord, m_seed);

    // Lakes before decoration (vanilla populate order): scattered self-sealing
    // water/lava ponds dug into flat ground. Chunk-pure via origin replay (see
    // LakeGen.h); the lake mask lets the tree/plant gates avoid the pond,
    // exactly like the cave mask.
    auto lakeMask = std::make_unique<lakes::LakeMask>();
    lakes::Place(
        chunk, chunkCoord, m_seed, heightAt,
        [&](int wx, int wz) {
            const Biome b = biomeAt(wx, wz);
            return b == Biome::Ocean || b == Biome::DeepOcean;
        },
        [&](int wx, int wz) { return biomeAt(wx, wz) == Biome::Desert; }, mask.get(),
        lakeMask.get());

    // Decoration: every tree whose canopy can reach this chunk. Cells are
    // visited in the same (ascending) order from every chunk, so overlapping
    // trees resolve identically no matter which chunk generates first.
    const int minCellX = FloorDiv(origin.x - kCanopyRadius, kTreeCell);
    const int maxCellX = FloorDiv(origin.x + Chunk::kSize - 1 + kCanopyRadius, kTreeCell);
    const int minCellZ = FloorDiv(origin.z - kCanopyRadius, kTreeCell);
    const int maxCellZ = FloorDiv(origin.z + Chunk::kSize - 1 + kCanopyRadius, kTreeCell);

    for (int cz = minCellZ; cz <= maxCellZ; ++cz) {
        for (int cx = minCellX; cx <= maxCellX; ++cx) {
            const int tx = cx * kTreeCell + static_cast<int>(Hash(m_seed, cx, cz, 2) % kTreeCell);
            const int tz = cz * kTreeCell + static_cast<int>(Hash(m_seed, cx, cz, 3) % kTreeCell);
            // Density follows the biome at the tree's own position, so the
            // gate stays identical from every chunk that can see the tree.
            const Biome treeBiome = biomeAt(tx, tz);
            const float chance = TreeChance(treeBiome);
            if (chance <= 0.0f || Hash01(m_seed, cx, cz, 1) > chance) {
                continue;
            }
            const int ground = heightAt(tx, tz);
            if (ground <= kBeachTop) {
                continue; // grass only — no beach or underwater trees
            }
            if (lakeMask->InLake(tx - origin.x, tz - origin.z)) {
                continue; // a lake claimed this column — no trees in the pond
            }
            // Species: snowy climate and alpine caps grow spruce; forests
            // mix in birch; everything else is oak. Spruce/birch run a
            // little taller than oak's 4..6.
            TreeSpecies species = TreeSpecies::Oak;
            if (treeBiome == Biome::Snowy || ground >= kSnowLine) {
                species = TreeSpecies::Spruce;
            } else if (treeBiome == Biome::Forest && Hash01(m_seed, cx, cz, 7) < 0.25f) {
                species = TreeSpecies::Birch;
            }
            const int base = species == TreeSpecies::Spruce ? 6
                             : species == TreeSpecies::Birch ? 5
                                                             : 4;
            const int trunk = base + static_cast<int>(Hash(m_seed, cx, cz, 4) % 3);
            if (ground + trunk + 2 >= kWorldHeightBlocks) {
                continue;
            }
            if (mask->Carved(tx - origin.x, ground - origin.y, tz - origin.z)) {
                continue; // a cave opened the ground cell — no floating trees
            }
            PlaceTree(chunk, origin, tx, ground, tz, trunk, species);
        }
    }

    // M16 plants: per-column hash gates, after trees so the air check sees
    // trunks. A plant only ever touches its own cell, so unlike trees no
    // neighbor enumeration is needed — chunk-local placement is already
    // seam-deterministic. Same carve veto: no flowers hovering over cave
    // mouths whose surface regrew lower.
    for (int z = 0; z < Chunk::kSize; ++z) {
        for (int x = 0; x < Chunk::kSize; ++x) {
            const int wx = origin.x + x;
            const int wz = origin.z + z;
            const int height = heightAt(wx, wz);
            const int ly = height + 1 - origin.y;
            // A plant fills one cell; a cactus up to three, so a column
            // based just below this chunk can still reach into it.
            if (ly >= Chunk::kSize || ly < -2) {
                continue;
            }
            if (mask->Carved(x, height - origin.y, z)) {
                continue;
            }
            if (lakeMask->InLake(x, z)) {
                continue; // no plants growing out of a pond
            }
            const Biome biome = biomeAt(wx, wz);
            const float r = Hash01(m_seed, wx, wz, 5);
            if (biome == Biome::Desert) {
                if (r < 0.015f) {
                    if (ly >= 0 && chunk.Get(x, ly, z) == blocks::Air) {
                        chunk.Set(x, ly, z, blocks::DeadBush);
                    }
                } else if (r < 0.020f && height > kBeachTop) {
                    // Cactus column, 1-3 tall, derived purely from
                    // (wx, wz, height): a column straddling a vertical
                    // chunk seam regenerates identically in both chunks —
                    // each writes the cells that fall inside it.
                    const int tall = 1 + static_cast<int>(Hash(m_seed, wx, wz, 8) % 3);
                    for (int k = 0; k < tall; ++k) {
                        const int cy = ly + k;
                        if (cy >= 0 && cy < Chunk::kSize &&
                            chunk.Get(x, cy, z) == blocks::Air) {
                            chunk.Set(x, cy, z, blocks::Cactus);
                        }
                    }
                }
                continue;
            }
            if (ly < 0 || height <= kBeachTop || chunk.Get(x, ly, z) != blocks::Air) {
                continue; // beaches stay bare
            }
            // Snowy surfaces (biome or alpine cap) get sparse grass only;
            // flowers keep to the green biomes.
            const bool snowy = biome == Biome::Snowy || height >= kSnowLine;
            const float grass = snowy ? 0.02f : biome == Biome::Forest ? 0.06f : 0.10f;
            const float flower = snowy ? 0.0f : biome == Biome::Forest ? 0.006f : 0.012f;
            BlockId plant = blocks::Air;
            if (r < grass) {
                plant = blocks::TallGrass;
            } else if (r < grass + flower) {
                plant = Hash(m_seed, wx, wz, 6) & 1 ? blocks::Dandelion : blocks::Poppy;
            }
            if (plant != blocks::Air) {
                chunk.Set(x, ly, z, plant);
            }
        }
    }
}

} // namespace vc
