#include "world/LakeGen.h"

#include <array>
#include <cstdint>

#include "world/Block.h"
#include "world/TerrainGen.h" // kSeaLevel

namespace vc::lakes {

namespace {

// Blob dimensions, straight from WorldGenLakes (16 x 16 x 8, bottom half
// liquid / top half air). The carve only ever sets interior cells (1..14 in
// XZ), leaving a >=1-column solid margin inside the footprint.
constexpr int kBlobXZ = 16;
constexpr int kBlobY = 8;
constexpr int kLiquidHalf = 4;

// Placement rarity, per chunk (vanilla: water 1/4, lava 1/80). Tuned a little
// commoner than vanilla to offset the lakes dropped by the single-vertical-
// chunk fit + cave rejects below, and to keep a surface lava pond findable.
constexpr int kWaterLakeInN = 3;
constexpr int kLavaLakeInN = 40;

// Reject lakes where the surface varies more than this across the footprint —
// keeps them in flat depressions instead of gouging pits into hillsides
// (vanilla's seal check has the same practical effect).
constexpr int kMaxRelief = 6;

// Small deterministic PRNG (SplitMix64). We don't need java.util.Random
// bit-parity (we aren't matching a vanilla world seed) — only determinism and
// nicely varied blobs.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t Next() {
        s += 0x9E3779B97F4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    int NextInt(int n) { return static_cast<int>(Next() % static_cast<uint64_t>(n)); }
    double NextDouble() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
};

uint64_t OriginSeed(int seed, int ox, int oz, int salt) {
    uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(seed)) * 0x9E3779B97F4A7C15ull;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(ox)) * 0xC2B2AE3D27D4EB4Full;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(oz)) * 0x165667B19E3779F9ull;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(salt)) * 0xD6E8FEB86659FD93ull;
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    return h;
}

using Blob = std::array<bool, static_cast<size_t>(kBlobXZ) * kBlobXZ * kBlobY>;

bool At(const Blob& b, int l, int i1, int j1) {
    return b[(static_cast<size_t>(l) * kBlobXZ + i1) * kBlobY + j1];
}

// Build the blob: 4-7 overlapping ellipsoids, copied from
// WorldGenLakes.generate's carve loop (interior cells 1..14 / 1..6 only).
void BuildBlob(Rng& rng, Blob& cells) {
    cells.fill(false);
    const int n = rng.NextInt(4) + 4;
    for (int j = 0; j < n; ++j) {
        const double d0 = rng.NextDouble() * 6.0 + 3.0;
        const double d1 = rng.NextDouble() * 4.0 + 2.0;
        const double d2 = rng.NextDouble() * 6.0 + 3.0;
        const double d3 = rng.NextDouble() * (16.0 - d0 - 2.0) + 1.0 + d0 / 2.0;
        const double d4 = rng.NextDouble() * (8.0 - d1 - 4.0) + 2.0 + d1 / 2.0;
        const double d5 = rng.NextDouble() * (16.0 - d2 - 2.0) + 1.0 + d2 / 2.0;
        for (int l = 1; l < 15; ++l) {
            for (int i1 = 1; i1 < 15; ++i1) {
                for (int j1 = 1; j1 < 7; ++j1) {
                    const double d6 = (static_cast<double>(l) - d3) / (d0 / 2.0);
                    const double d7 = (static_cast<double>(j1) - d4) / (d1 / 2.0);
                    const double d8 = (static_cast<double>(i1) - d5) / (d2 / 2.0);
                    if (d6 * d6 + d7 * d7 + d8 * d8 < 1.0) {
                        cells[(static_cast<size_t>(l) * kBlobXZ + i1) * kBlobY + j1] = true;
                    }
                }
            }
        }
    }
}

// One lake candidate, anchored to origin chunk (ox, oz). The accept decision
// (gate + climate + flatness) is a pure function of (seed, origin, heightAt),
// so every chunk agrees; the footprint is marked into `mask` for decoration
// veto. The blob is built + written only by the origin chunk itself (the
// footprint never spills past it), so writes are seam-free by construction.
void Consider(Chunk& chunk, const glm::ivec3& chunkCoord, int seed, int ox, int oz,
              BlockId liquid, int oneInN, int salt, bool skipDesert,
              const std::function<int(int, int)>& heightAt,
              const std::function<bool(int, int)>& isOcean,
              const std::function<bool(int, int)>& isDesert, const caves::CarveMask* carveMask,
              LakeMask* mask) {
    Rng rng(OriginSeed(seed, ox, oz, salt));
    if (rng.NextInt(oneInN) != 0) {
        return; // density gate
    }
    const int cornerX = ox * Chunk::kSize;
    const int cornerZ = oz * Chunk::kSize;
    const int centerX = cornerX + kBlobXZ / 2;
    const int centerZ = cornerZ + kBlobXZ / 2;

    // Dry land clearly above the shoreline, never in ocean or (water) desert.
    if (isOcean(centerX, centerZ)) {
        return;
    }
    if (skipDesert && isDesert(centerX, centerZ)) {
        return;
    }
    const int centerGround = heightAt(centerX, centerZ);
    if (centerGround <= TerrainGenerator::kSeaLevel + 2) {
        return;
    }

    // Cheap early-out: a lake (and the decoration that must avoid it) lives
    // within a band around the surface. Skip the expensive footprint scan for
    // vertical chunks nowhere near it — only the surface chunk(s) do real work.
    const int chunkMinY = chunkCoord.y * Chunk::kSize;
    if (centerGround < chunkMinY - (kMaxRelief + kLiquidHalf + kBlobY) ||
        centerGround > chunkMinY + Chunk::kSize + kLiquidHalf) {
        return;
    }

    // Anchor below the LOWEST surface in the footprint so the basin self-seals
    // (every non-blob neighbour of a water cell is below every column's
    // surface => solid). Reject hilly spots so lakes sit in flat depressions.
    int minGround = 1 << 30;
    int maxGround = -(1 << 30);
    for (int l = 0; l < kBlobXZ; ++l) {
        for (int i1 = 0; i1 < kBlobXZ; ++i1) {
            const int g = heightAt(cornerX + l, cornerZ + i1);
            minGround = g < minGround ? g : minGround;
            maxGround = g > maxGround ? g : maxGround;
        }
    }
    if (maxGround - minGround > kMaxRelief) {
        return;
    }

    // Constrain the 8-tall blob to a single vertical chunk so one chunk owns
    // and writes the whole lake (and can cave-check it locally). Pure in
    // blobMinY, so every chunk agrees on which lakes survive.
    const int blobMinY = minGround - kLiquidHalf; // water top at minGround-1
    if (blobMinY / Chunk::kSize != (blobMinY + kBlobY - 1) / Chunk::kSize) {
        return;
    }

    // Accepted: claim the footprint columns so trees/plants avoid the pond.
    const glm::ivec3 origin = chunkCoord * Chunk::kSize;
    if (mask != nullptr) {
        for (int l = 0; l < kBlobXZ; ++l) {
            for (int i1 = 0; i1 < kBlobXZ; ++i1) {
                mask->Mark(cornerX + l - origin.x, cornerZ + i1 - origin.z);
            }
        }
    }

    // Only the one owning chunk writes the blob (XZ origin chunk + the vertical
    // chunk that contains it). The footprint never reaches an XZ neighbour, and
    // the blob never reaches a vertical neighbour.
    if (ox != chunkCoord.x || oz != chunkCoord.z ||
        blobMinY / Chunk::kSize != chunkCoord.y) {
        return;
    }

    // Reject lakes that intersect a cave: any open (carved) cell in the
    // liquid band or its floor would let the pond leak. The whole blob is in
    // this chunk, so its carve mask covers the check.
    for (int l = 0; l < kBlobXZ; ++l) {
        for (int i1 = 0; i1 < kBlobXZ; ++i1) {
            for (int wy = blobMinY; wy < blobMinY + kLiquidHalf; ++wy) {
                if (carveMask != nullptr &&
                    carveMask->Carved(cornerX + l - origin.x, wy - origin.y,
                                      cornerZ + i1 - origin.z)) {
                    return;
                }
            }
        }
    }

    Blob cells;
    BuildBlob(rng, cells);

    // Lower half -> liquid (the basin), upper half -> air (the open bowl).
    // Never disturb bedrock.
    for (int l = 0; l < kBlobXZ; ++l) {
        for (int i1 = 0; i1 < kBlobXZ; ++i1) {
            for (int j1 = 0; j1 < kBlobY; ++j1) {
                if (!At(cells, l, i1, j1)) {
                    continue;
                }
                const int lx = cornerX + l - origin.x;
                const int ly = blobMinY + j1 - origin.y;
                const int lz = cornerZ + i1 - origin.z;
                if (lx < 0 || lx >= Chunk::kSize || ly < 0 || ly >= Chunk::kSize || lz < 0 ||
                    lz >= Chunk::kSize) {
                    continue;
                }
                if (chunk.Get(lx, ly, lz) == blocks::Bedrock) {
                    continue;
                }
                chunk.Set(lx, ly, lz, j1 < kLiquidHalf ? liquid : blocks::Air);
            }
        }
    }
}

} // namespace

void Place(Chunk& chunk, const glm::ivec3& chunkCoord, int seed,
           const std::function<int(int, int)>& heightAt,
           const std::function<bool(int, int)>& isOcean,
           const std::function<bool(int, int)>& isDesert, const caves::CarveMask* carveMask,
           LakeMask* mask) {
    for (int oz = chunkCoord.z - 1; oz <= chunkCoord.z + 1; ++oz) {
        for (int ox = chunkCoord.x - 1; ox <= chunkCoord.x + 1; ++ox) {
            Consider(chunk, chunkCoord, seed, ox, oz, blocks::Water, kWaterLakeInN, 31,
                     /*skipDesert=*/true, heightAt, isOcean, isDesert, carveMask, mask);
            Consider(chunk, chunkCoord, seed, ox, oz, blocks::Lava, kLavaLakeInN, 47,
                     /*skipDesert=*/false, heightAt, isOcean, isDesert, carveMask, mask);
        }
    }
}

} // namespace vc::lakes
