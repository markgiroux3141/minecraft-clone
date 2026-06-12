// Structural regression test for terrain generation + decoration, plus a
// mesher smoke test of the packed vertex format. Chunks generate
// independently, so every tree must come out identical no matter which
// chunk regenerates which part of it — seam bugs show up as floating
// leaves or truncated trunks. Exits 0 on pass, 1 on the first failure.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include <glm/glm.hpp>

#include "vox/core/Log.h"

#include "world/Block.h"
#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/Light.h"
#include "world/TerrainGen.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "  ok " : "FAIL ", what);
    if (!ok) {
        ++g_failures;
    }
}

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
               static_cast<size_t>(v.z) * 83492791u;
    }
};

} // namespace

int main() {
    vox::Log::Init();
    vc::blocks::RegisterDefaults();

    constexpr int kSeed = 1337;
    constexpr int kRadius = 3; // chunks; region spans [-48, 64) in x/z
    const vc::TerrainGenerator gen(kSeed);

    std::unordered_map<glm::ivec3, vc::Chunk, IVec3Hash> region;
    for (int cz = -kRadius; cz <= kRadius; ++cz) {
        for (int cx = -kRadius; cx <= kRadius; ++cx) {
            for (int cy = 0; cy < vc::kWorldHeightChunks; ++cy) {
                gen.Generate(region[{cx, cy, cz}], {cx, cy, cz});
            }
        }
    }

    const auto blockAt = [&](int wx, int wy, int wz) -> vc::BlockId {
        if (wy < 0 || wy >= vc::kWorldHeightBlocks) {
            return vc::blocks::Air;
        }
        const glm::ivec3 coord{wx >> 4, wy >> 4, wz >> 4};
        const auto it = region.find(coord);
        return it == region.end() ? vc::blocks::Air
                                  : it->second.Get(wx & 15, wy & 15, wz & 15);
    };

    // Re-generating a chunk must be bit-identical (threaded gen relies on it).
    {
        vc::Chunk again;
        gen.Generate(again, {1, 2, -1});
        Check(again.Raw() == region.at({1, 2, -1}).Raw(), "generation is deterministic");
    }

    // Structural sweep over the interior (margin so canopies near the region
    // edge don't reference ungenerated chunks).
    const int lo = -kRadius * vc::Chunk::kSize + vc::Chunk::kSize;
    const int hi = (kRadius + 1) * vc::Chunk::kSize - vc::Chunk::kSize;
    const auto isLog = [](vc::BlockId id) {
        return id == vc::blocks::Log || id == vc::blocks::BirchLog ||
               id == vc::blocks::SpruceLog;
    };
    const auto isLeaves = [](vc::BlockId id) {
        return id == vc::blocks::Leaves || id == vc::blocks::BirchLeaves ||
               id == vc::blocks::SpruceLeaves;
    };
    size_t logs = 0;
    size_t leafBlocks = 0;
    bool trunksIntact = true;
    bool leavesAnchored = true;
    for (int wz = lo; wz < hi; ++wz) {
        for (int wx = lo; wx < hi; ++wx) {
            for (int wy = 1; wy < vc::kWorldHeightBlocks - 1; ++wy) {
                const vc::BlockId id = blockAt(wx, wy, wz);
                if (isLog(id)) {
                    ++logs;
                    // A trunk runs dirt -> logs -> canopy with no gaps; a
                    // vertical-seam bug would leave air above or below.
                    const vc::BlockId below = blockAt(wx, wy - 1, wz);
                    const vc::BlockId above = blockAt(wx, wy + 1, wz);
                    if (!isLog(below) && below != vc::blocks::Dirt) {
                        trunksIntact = false;
                    }
                    if (!isLog(above) && !isLeaves(above)) {
                        trunksIntact = false;
                    }
                } else if (isLeaves(id)) {
                    ++leafBlocks;
                    // Every canopy block sits within 2 blocks of its trunk; a
                    // horizontal-seam bug strands leaves with no log nearby.
                    bool anchored = false;
                    for (int dy = -2; dy <= 2 && !anchored; ++dy) {
                        for (int dz = -2; dz <= 2 && !anchored; ++dz) {
                            for (int dx = -2; dx <= 2 && !anchored; ++dx) {
                                anchored = isLog(blockAt(wx + dx, wy + dy, wz + dz));
                            }
                        }
                    }
                    leavesAnchored &= anchored;
                }
            }
        }
    }
    Check(logs > 0 && leafBlocks > 0, "trees actually generate");
    Check(trunksIntact, "trunks are contiguous across chunk seams");
    Check(leavesAnchored, "every leaf block has a trunk within reach");

    // Plants (M16): cross-meshed decoration must stand on its proper soil
    // (earth for grass/flowers, sand for dead bushes) — a floating plant
    // means the carve veto or the surface rules broke.
    {
        size_t plants = 0;
        bool rooted = true;
        for (int wz = lo; wz < hi; ++wz) {
            for (int wx = lo; wx < hi; ++wx) {
                for (int wy = 1; wy < vc::kWorldHeightBlocks - 1; ++wy) {
                    const vc::BlockId id = blockAt(wx, wy, wz);
                    const vc::BlockId below = blockAt(wx, wy - 1, wz);
                    if (id == vc::blocks::TallGrass || id == vc::blocks::Dandelion ||
                        id == vc::blocks::Poppy) {
                        ++plants;
                        rooted &= below == vc::blocks::Grass || below == vc::blocks::Dirt ||
                                  below == vc::blocks::SnowyGrass;
                    } else if (id == vc::blocks::DeadBush) {
                        ++plants;
                        rooted &= below == vc::blocks::Sand;
                    } else if (id == vc::blocks::Cactus) {
                        ++plants;
                        rooted &= below == vc::blocks::Sand || below == vc::blocks::Cactus;
                    }
                }
            }
        }
        Check(plants > 0, "plants actually generate");
        Check(rooted, "every plant sits on its proper soil");
    }

    // Caves (M15): carved air shows up as air with stone directly above
    // (heightmap terrain has no other overhangs). The breach invariant:
    // below sea level the only air is carved, and the carver's analytic
    // water test guarantees it never touches worldgen water — a violation
    // means cave-into-ocean leaks (waterfalls at chunk seams).
    size_t caveAir = 0;
    bool noOceanBreach = true;
    for (int wz = lo; wz < hi; ++wz) {
        for (int wx = lo; wx < hi; ++wx) {
            for (int wy = 1; wy < vc::kWorldHeightBlocks - 1; ++wy) {
                if (blockAt(wx, wy, wz) != vc::blocks::Air) {
                    continue;
                }
                if (blockAt(wx, wy + 1, wz) == vc::blocks::Stone) {
                    ++caveAir;
                }
                if (wy <= vc::TerrainGenerator::kSeaLevel) {
                    noOceanBreach &= blockAt(wx + 1, wy, wz) != vc::blocks::Water &&
                                     blockAt(wx - 1, wy, wz) != vc::blocks::Water &&
                                     blockAt(wx, wy, wz + 1) != vc::blocks::Water &&
                                     blockAt(wx, wy, wz - 1) != vc::blocks::Water &&
                                     blockAt(wx, wy + 1, wz) != vc::blocks::Water &&
                                     blockAt(wx, wy - 1, wz) != vc::blocks::Water;
                }
            }
        }
    }
    Check(caveAir > 0, "caves actually carve");
    Check(noOceanBreach, "carved air never touches worldgen water");

    // Ores (M21): coal and iron veins generate, iron stays in the low
    // band (the "dig deeper" rule), and every ore cell is buried like the
    // stone it replaced — an ore floating in air or water would mean the
    // carve/ore order or the stone-only predicate broke.
    {
        size_t coal = 0;
        size_t iron = 0;
        int ironMaxY = 0;
        bool oreInStone = true;
        for (int wz = lo; wz < hi; ++wz) {
            for (int wx = lo; wx < hi; ++wx) {
                for (int wy = 1; wy < vc::kWorldHeightBlocks - 1; ++wy) {
                    const vc::BlockId id = blockAt(wx, wy, wz);
                    if (id != vc::blocks::CoalOre && id != vc::blocks::IronOre) {
                        continue;
                    }
                    id == vc::blocks::CoalOre ? ++coal : ++iron;
                    if (id == vc::blocks::IronOre) {
                        ironMaxY = std::max(ironMaxY, wy);
                    }
                    // Veins only replace stone, so an ore cell must touch
                    // at least one solid neighbor on every axis pair —
                    // cheap proxy: it can't be fully surrounded by air.
                    bool buried = false;
                    constexpr int kD[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                              {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
                    for (const auto& d : kD) {
                        const vc::BlockId n = blockAt(wx + d[0], wy + d[1], wz + d[2]);
                        buried |= n != vc::blocks::Air && n != vc::blocks::Water;
                    }
                    oreInStone &= buried;
                }
            }
        }
        Check(coal > 0, "coal veins actually generate");
        Check(iron > 0, "iron veins actually generate");
        Check(ironMaxY < 32, "iron stays in the deep band");
        Check(oreInStone, "ore cells sit in terrain, not open space");
    }

    // Bedrock floor: solid at y0, ragged only through y4, nowhere else —
    // neither players (unbreakable) nor caves (not replaceable) can open
    // the void below the world.
    {
        bool floorSolid = true;
        bool bedrockBounded = true;
        for (int wz = lo; wz < hi; ++wz) {
            for (int wx = lo; wx < hi; ++wx) {
                floorSolid &= blockAt(wx, 0, wz) == vc::blocks::Bedrock;
                for (int wy = 5; wy < vc::kWorldHeightBlocks; ++wy) {
                    bedrockBounded &= blockAt(wx, wy, wz) != vc::blocks::Bedrock;
                }
            }
        }
        Check(floorSolid, "bedrock floor is solid at y0");
        Check(bedrockBounded, "no bedrock above y4");
    }

    // Biomes (M15): sample columns ~512 blocks apart; the climate fields
    // (~300-block features) must produce more than one surface family over
    // a 2-km span. Also: snowy surfaces always sit on dirt (not stone).
    {
        bool sawGrass = false;
        bool sawSnowOrDesert = false;
        bool snowOnDirt = true;
        size_t sandColumns = 0;
        size_t sandstoneColumns = 0;
        for (int scz = -64; scz <= 64; scz += 32) {
            for (int scx = -64; scx <= 64; scx += 32) {
                std::array<vc::Chunk, vc::kWorldHeightChunks> column;
                for (int cy = 0; cy < vc::kWorldHeightChunks; ++cy) {
                    gen.Generate(column[static_cast<size_t>(cy)], {scx, cy, scz});
                }
                const auto columnBlock = [&](int lx, int wy, int lz) {
                    return column[static_cast<size_t>(wy >> 4)].Get(lx, wy & 15, lz);
                };
                for (int wy = vc::kWorldHeightBlocks - 1; wy > 0; --wy) {
                    const vc::BlockId id = columnBlock(8, wy, 8);
                    if (id == vc::blocks::Air || id == vc::blocks::Water) {
                        continue;
                    }
                    sawGrass |= id == vc::blocks::Grass;
                    sawSnowOrDesert |= id == vc::blocks::SnowyGrass || id == vc::blocks::Sand;
                    if (id == vc::blocks::SnowyGrass) {
                        snowOnDirt &= columnBlock(8, wy - 1, 8) == vc::blocks::Dirt;
                    }
                    // Sandy columns: 3 blocks of sand, then a 0..3-block
                    // sandstone band (vanilla's anti-floating-sand buffer).
                    if (id == vc::blocks::Sand) {
                        ++sandColumns;
                        for (int dy = 3; dy <= 6 && wy - dy > 0; ++dy) {
                            if (columnBlock(8, wy - dy, 8) == vc::blocks::Sandstone) {
                                ++sandstoneColumns;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
        Check(sawGrass && sawSnowOrDesert, "climate produces multiple biomes");
        Check(snowOnDirt, "snowy grass sits on dirt");
        // nextInt(4)-deep band: ~3/4 of sandy columns have one. Require
        // presence without demanding every column (0-depth rolls are legal).
        Check(sandColumns == 0 || sandstoneColumns > 0,
              "sandstone band generates under sand surfaces");
    }

    // Mesher smoke test: a lone stone block and a lone water block in an
    // otherwise empty snapshot. Decodes the packed vertex stream the same
    // way chunk.vert does.
    {
        vc::ChunkSnapshot snapshot;
        auto center = std::make_shared<vc::Chunk>();
        center->Set(8, 8, 8, vc::blocks::Stone);
        center->Set(4, 4, 4, vc::blocks::Water);
        snapshot.chunks[vc::ChunkSnapshot::Index(1, 1, 1)] = center;
        snapshot.skyAbove = true;

        const vc::ChunkMesh mesh = vc::ChunkMesher::Build(snapshot);
        Check(mesh.vertices.size() == 24, "lone stone block meshes to 6 quads");
        Check(mesh.transparentVertices.size() == 24, "lone water block meshes transparently");

        const auto cornersOk = [](const std::vector<vc::ChunkVertex>& vertices, uint32_t lo) {
            bool ok = true;
            for (const auto& vertex : vertices) {
                const uint32_t x = vertex.data0 & 31u;
                const uint32_t y = (vertex.data0 >> 5) & 31u;
                const uint32_t z = (vertex.data0 >> 10) & 31u;
                const uint32_t normal = (vertex.data0 >> 15) & 7u;
                const uint32_t u = vertex.data1 & 31u;
                const uint32_t v = (vertex.data1 >> 5) & 31u;
                ok &= x >= lo && x <= lo + 1 && y >= lo && y <= lo + 1 && z >= lo &&
                      z <= lo + 1 && normal < 6 && u <= 1 && v <= 1;
            }
            return ok;
        };
        Check(cornersOk(mesh.vertices, 8), "packed stone vertices decode to the block corners");
        Check(cornersOk(mesh.transparentVertices, 4),
              "packed water vertices decode to the block corners");
    }

    // Torch mesh (M21 follow-up): four inset one-sided planes riding the
    // spare packed bits — every vertex carries exactly one sub-block
    // inset code (1 = 7/16 or 2 = 9/16) on exactly one axis, anchored to
    // the torch's own cell corners.
    {
        vc::ChunkSnapshot snapshot;
        auto center = std::make_shared<vc::Chunk>();
        center->Set(8, 8, 8, vc::blocks::Torch);
        snapshot.chunks[vc::ChunkSnapshot::Index(1, 1, 1)] = center;
        snapshot.skyAbove = true;

        const vc::ChunkMesh mesh = vc::ChunkMesher::Build(snapshot);
        Check(mesh.vertices.size() == 16, "lone torch meshes to 4 planes");
        bool insetsOk = true;
        for (const auto& vertex : mesh.vertices) {
            const uint32_t x = vertex.data0 & 31u;
            const uint32_t y = (vertex.data0 >> 5) & 31u;
            const uint32_t z = (vertex.data0 >> 10) & 31u;
            const uint32_t xIn = (vertex.data0 >> 28) & 3u;
            const uint32_t zIn = vertex.data0 >> 30;
            insetsOk &= x >= 8 && x <= 9 && y >= 8 && y <= 9 && z >= 8 && z <= 9;
            insetsOk &= (xIn == 0) != (zIn == 0); // exactly one axis inset
            insetsOk &= xIn != 3 && zIn != 3;     // codes are 1 or 2 only
        }
        Check(insetsOk, "torch vertices carry one inset code on one axis");
    }

    if (g_failures == 0) {
        std::printf("GenTest: all checks passed (%zu logs, %zu leaves)\n", logs, leafBlocks);
    } else {
        std::printf("GenTest: %d check(s) FAILED\n", g_failures);
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
