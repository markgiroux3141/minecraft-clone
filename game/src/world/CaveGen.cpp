#include "world/CaveGen.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "world/JavaRandom.h"
#include "world/Light.h" // kWorldHeightBlocks
#include "world/TerrainGen.h"

namespace vc::caves {

namespace {

using vc::JavaRandom;

// Origin-chunk radius checked around the target chunk; tunnels can wander
// up to range*16-16 blocks, which is why every chunk must replay the
// tunnels of a 17x17 chunk neighborhood.
constexpr int kRange = 8;

constexpr float kPi = std::numbers::pi_v<float>;

struct Carver {
    Chunk& chunk;
    const glm::ivec3 chunkCoord; // target chunk (3D; vanilla columns are 2D)
    const std::function<int(int, int)>& heightAt;
    CarveMask* mask; // may be null

    // Worldgen water exists at (wx, wy, wz) iff heightAt < wy <= sea level.
    // Scanning the rule instead of chunk contents keeps the breach decision
    // identical from every chunk (vanilla scans the primer clipped to the
    // current column, which can leak caves into oceans at chunk borders).
    bool WaterInBox(int wx0, int wx1, int wz0, int wz1, int wy0, int wy1) const {
        const int yHigh = std::min(wy1, TerrainGenerator::kSeaLevel);
        if (yHigh < wy0) {
            return false; // box entirely above sea level
        }
        for (int wz = wz0; wz <= wz1; ++wz) {
            for (int wx = wx0; wx <= wx1; ++wx) {
                if (heightAt(wx, wz) < yHigh) {
                    return true;
                }
            }
        }
        return false;
    }

    void AddTunnel(int64_t tunnelSeed, double x, double y, double z, float radius, float yaw,
                   float pitch, int step, int totalSteps, double squash) {
        const double centerX = chunkCoord.x * 16 + 8;
        const double centerZ = chunkCoord.z * 16 + 8;
        float yawDelta = 0.0f;
        float pitchDelta = 0.0f;
        JavaRandom random(tunnelSeed);

        if (totalSteps <= 0) {
            const int max = kRange * 16 - 16;
            totalSteps = max - random.NextInt(max / 4);
        }
        bool isRoom = false;
        if (step == -1) {
            step = totalSteps / 2;
            isRoom = true;
        }
        const int branchStep = random.NextInt(totalSteps / 2) + totalSteps / 4;

        for (const bool gentlePitch = random.NextInt(6) == 0; step < totalSteps; ++step) {
            // Radius bulges along the tunnel (sine over progress); squash
            // flattens rooms vertically.
            const double radH = 1.5 + std::sin(static_cast<float>(step) * kPi /
                                               static_cast<float>(totalSteps)) *
                                          radius;
            const double radV = radH * squash;
            const float cosPitch = std::cos(pitch);
            x += std::cos(yaw) * cosPitch;
            y += std::sin(pitch);
            z += std::sin(yaw) * cosPitch;
            pitch *= gentlePitch ? 0.92f : 0.7f;
            pitch += pitchDelta * 0.1f;
            yaw += yawDelta * 0.1f;
            pitchDelta *= 0.9f;
            yawDelta *= 0.75f;
            pitchDelta += (random.NextFloat() - random.NextFloat()) * random.NextFloat() * 2.0f;
            yawDelta += (random.NextFloat() - random.NextFloat()) * random.NextFloat() * 4.0f;

            // Wide tunnels split into two perpendicular branches mid-walk.
            if (!isRoom && step == branchStep && radius > 1.0f && totalSteps > 0) {
                AddTunnel(random.NextLong(), x, y, z, random.NextFloat() * 0.5f + 0.5f,
                          yaw - kPi / 2.0f, pitch / 3.0f, step, totalSteps, 1.0);
                AddTunnel(random.NextLong(), x, y, z, random.NextFloat() * 0.5f + 0.5f,
                          yaw + kPi / 2.0f, pitch / 3.0f, step, totalSteps, 1.0);
                return;
            }
            // Tunnels only carve 3 of 4 steps (rooms always do). Mind the
            // short-circuit: rooms must not draw from the RNG here.
            if (!isRoom && random.NextInt(4) == 0) {
                continue;
            }

            // Give up once the walk provably can't reach the target chunk.
            const double dx = x - centerX;
            const double dz = z - centerZ;
            const double stepsLeft = totalSteps - step;
            const double maxReach = radius + 2.0f + 16.0f;
            if (dx * dx + dz * dz - stepsLeft * stepsLeft > maxReach * maxReach) {
                return;
            }
            if (x < centerX - 16.0 - radH * 2.0 || z < centerZ - 16.0 - radH * 2.0 ||
                x > centerX + 16.0 + radH * 2.0 || z > centerZ + 16.0 + radH * 2.0) {
                continue;
            }

            // Sphere bounding box: x/z local to the target chunk (widened to
            // the mask skirt), y in world blocks (vanilla clamps y to
            // [1, top-8]; ours keeps a 1-block world floor the same way).
            const glm::ivec3 org = chunkCoord * Chunk::kSize;
            const int pad = mask ? CarveMask::kPad : 0;
            const int padDown = mask ? CarveMask::kPadDown : 0;
            int x0 = static_cast<int>(std::floor(x - radH)) - org.x - 1;
            int x1 = static_cast<int>(std::floor(x + radH)) - org.x + 1;
            int wy0 = static_cast<int>(std::floor(y - radV)) - 1;
            int wy1 = static_cast<int>(std::floor(y + radV)) + 1;
            int z0 = static_cast<int>(std::floor(z - radH)) - org.z - 1;
            int z1 = static_cast<int>(std::floor(z + radH)) - org.z + 1;
            x0 = std::max(x0, -pad);
            x1 = std::min(x1, Chunk::kSize + pad);
            wy0 = std::max(wy0, 1);
            wy1 = std::min(wy1, kWorldHeightBlocks - 2);
            z0 = std::max(z0, -pad);
            z1 = std::min(z1, Chunk::kSize + pad);

            // Ocean-breach test over the UNCLIPPED box (see WaterInBox).
            if (WaterInBox(static_cast<int>(std::floor(x - radH)) - 1,
                           static_cast<int>(std::floor(x + radH)) + 1,
                           static_cast<int>(std::floor(z - radH)) - 1,
                           static_cast<int>(std::floor(z + radH)) + 1, wy0 - 1, wy1 + 1)) {
                continue;
            }

            for (int lx = x0; lx < x1; ++lx) {
                const double nx = (lx + org.x + 0.5 - x) / radH;
                for (int lz = z0; lz < z1; ++lz) {
                    const double nz = (lz + org.z + 0.5 - z) / radH;
                    if (nx * nx + nz * nz >= 1.0) {
                        continue;
                    }
                    BlockId surfaceId = blocks::Air; // grass-family block carved above
                    for (int wy = wy1; wy > wy0; --wy) {
                        // Vanilla quirk kept as-is: the ellipsoid test uses
                        // the cell below the one carved, which flattens cave
                        // floors (the -0.7 cutoff) and lifts ceilings.
                        const double ny = (wy - 1 + 0.5 - y) / radV;
                        if (ny <= -0.7 || nx * nx + ny * ny + nz * nz >= 1.0) {
                            continue;
                        }
                        const int ly = wy - org.y;
                        if (mask && lx >= -pad && lx < Chunk::kSize + pad && ly >= -padDown &&
                            ly < Chunk::kSize && lz >= -pad && lz < Chunk::kSize + pad) {
                            mask->bits.set(CarveMask::Index(lx, ly, lz));
                        }
                        if (lx < 0 || lx >= Chunk::kSize || ly < 0 || ly >= Chunk::kSize ||
                            lz < 0 || lz >= Chunk::kSize) {
                            continue; // skirt cell — another chunk carves it
                        }
                        const BlockId id = chunk.Get(lx, ly, lz);
                        if (id == blocks::Grass || id == blocks::SnowyGrass) {
                            surfaceId = id;
                        }
                        // Vanilla's replaceable set, reduced to our palette.
                        // (Its sand-under-water exception can't trigger: the
                        // breach test already rejected any water in the box.)
                        if (id != blocks::Stone && id != blocks::Dirt && id != blocks::Grass &&
                            id != blocks::SnowyGrass && id != blocks::Sand &&
                            id != blocks::Sandstone) {
                            continue;
                        }
                        // M26: deep carved cells fill with lava instead of
                        // air (vanilla MapGenCaves.digBlock: lava when the
                        // block below sits under y10). Placed as a SOURCE so
                        // the pools sit statically on the cave floor, lighting
                        // it (emission 15) until the player disturbs them.
                        chunk.Set(lx, ly, lz, wy <= 10 ? blocks::Lava : blocks::Air);
                        // A carved-open surface column regrows its own kind
                        // of grass on the dirt that becomes the new top.
                        if (surfaceId != blocks::Air && ly > 0 &&
                            chunk.Get(lx, ly - 1, lz) == blocks::Dirt) {
                            chunk.Set(lx, ly - 1, lz, surfaceId);
                        }
                    }
                }
            }
            if (isRoom) {
                break;
            }
        }
    }

    // Per origin chunk: usually nothing; occasionally a burst of tunnels,
    // 1 in 4 of those anchored on a squashed room.
    void CarveOrigin(int ocx, int ocz, JavaRandom& rand) {
        int count = rand.NextInt(rand.NextInt(rand.NextInt(15) + 1) + 1);
        if (rand.NextInt(7) != 0) {
            count = 0;
        }
        for (int i = 0; i < count; ++i) {
            const double x = ocx * 16 + rand.NextInt(16);
            // Vanilla draws nextInt(nextInt(120)+8) under a 256 world; scaled
            // to our 128-block height (M25), same low bias — most tunnels sit
            // deep, a few reach up toward the ~y65 surface.
            const double y = rand.NextInt(rand.NextInt(108) + 8);
            const double z = ocz * 16 + rand.NextInt(16);
            int tunnels = 1;
            if (rand.NextInt(4) == 0) {
                AddTunnel(rand.NextLong(), x, y, z, 1.0f + rand.NextFloat() * 6.0f, 0.0f, 0.0f,
                          -1, -1, 0.5);
                tunnels += rand.NextInt(4);
            }
            for (int t = 0; t < tunnels; ++t) {
                const float yaw = rand.NextFloat() * (kPi * 2.0f);
                const float pitch = (rand.NextFloat() - 0.5f) * 2.0f / 8.0f;
                float radius = rand.NextFloat() * 2.0f + rand.NextFloat();
                if (rand.NextInt(10) == 0) {
                    radius *= rand.NextFloat() * rand.NextFloat() * 3.0f + 1.0f;
                }
                AddTunnel(rand.NextLong(), x, y, z, radius, yaw, pitch, 0, 0, 1.0);
            }
        }
    }
};

} // namespace

void Carve(Chunk& chunk, const glm::ivec3& chunkCoord, int seed,
           const std::function<int(int, int)>& heightAt, CarveMask* mask) {
    Carver carver{chunk, chunkCoord, heightAt, mask};
    JavaRandom rand(seed);
    const int64_t saltX = rand.NextLong();
    const int64_t saltZ = rand.NextLong();
    for (int ocx = chunkCoord.x - kRange; ocx <= chunkCoord.x + kRange; ++ocx) {
        for (int ocz = chunkCoord.z - kRange; ocz <= chunkCoord.z + kRange; ++ocz) {
            rand.SetSeed((static_cast<int64_t>(ocx) * saltX) ^
                         (static_cast<int64_t>(ocz) * saltZ) ^ static_cast<int64_t>(seed));
            carver.CarveOrigin(ocx, ocz, rand);
        }
    }
}

} // namespace vc::caves
