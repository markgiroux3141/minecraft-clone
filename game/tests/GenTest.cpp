// Structural regression test for terrain generation + decoration, plus a
// mesher smoke test of the packed vertex format. Chunks generate
// independently, so every tree must come out identical no matter which
// chunk regenerates which part of it — seam bugs show up as floating
// leaves or truncated trunks. Exits 0 on pass, 1 on the first failure.

#include <algorithm>
#include <cmath>
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
    // M26: caves below y10 fill with lava (vanilla MapGenCaves.digBlock).
    // M27 adds surface lava LAKES (always >= y56), so the only forbidden lava
    // is in the mid-depths [11, 50] — that would be cave lava leaking upward.
    size_t caveLava = 0;
    bool noMidLava = true;
    for (int wz = lo; wz < hi; ++wz) {
        for (int wx = lo; wx < hi; ++wx) {
            for (int wy = 1; wy < vc::kWorldHeightBlocks - 1; ++wy) {
                if (blockAt(wx, wy, wz) == vc::blocks::Lava) {
                    if (wy <= 10) {
                        ++caveLava;
                    } else if (wy <= 50) {
                        noMidLava = false;
                    }
                }
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
    Check(caveLava > 0, "lava pools generate on deep cave floors (below y10)");
    Check(noMidLava, "no cave lava leaks into the mid-depths (y11..50)");

    // Oceans (M27): the continentalness field (ocean biomes with negative
    // base height) drops whole regions below sea level so the M11 fill makes
    // open water. Oceans are large (~660-block features), so a fixed window
    // can miss them — scan a long transect that crosses several wavelengths.
    // An ocean column reads as Water at exactly kSeaLevel sitting on a solid
    // floor below it; depth = how far that floor sits under the sea.
    {
        const int sea = vc::TerrainGenerator::kSeaLevel;
        std::unordered_map<glm::ivec3, vc::Chunk, IVec3Hash> strip;
        for (int cx = -32; cx < 32; ++cx) {
            for (int cy = 1; cy <= 3; ++cy) { // y16..63: deep floors + water
                gen.Generate(strip[{cx, cy, 0}], {cx, cy, 0});
            }
        }
        const auto stripAt = [&](int wx, int wy, int wz) -> vc::BlockId {
            const glm::ivec3 coord{wx >> 4, wy >> 4, wz >> 4};
            const auto it = strip.find(coord);
            return it == strip.end() ? vc::blocks::Air : it->second.Get(wx & 15, wy & 15, wz & 15);
        };
        size_t oceanCols = 0;
        int maxDepth = 0;
        bool floorsSolid = true;
        for (int wx = -32 * 16 + 16; wx < 32 * 16 - 16; ++wx) {
            if (stripAt(wx, sea, 0) != vc::blocks::Water) {
                continue; // not an ocean/lake surface column
            }
            ++oceanCols;
            int floor = sea;
            while (floor > 16 && stripAt(wx, floor, 0) == vc::blocks::Water) {
                --floor;
            }
            // The block under the water must be solid (a sealed basin floor),
            // never air — air under sea-level water would be a breach.
            floorsSolid &= stripAt(wx, floor, 0) != vc::blocks::Air;
            maxDepth = std::max(maxDepth, sea - floor);
        }
        Check(oceanCols > 0, "oceans generate (open water over a sub-sea-level floor)");
        Check(floorsSolid, "ocean water sits on a solid floor (no breach)");
        Check(maxDepth >= 18, "deep ocean basins form (water >= 18 deep somewhere)");
    }

    // Lakes (M27 Part B): scattered self-sealing ponds dug into flat land,
    // ABOVE sea level (so distinct from oceans), reusing the structural region
    // above (no extra generation). The basin is anchored below the local
    // surface, so a correct lake never shows liquid with an exposed air face —
    // that invariant doubles as a seam test: a lake truncated at a chunk
    // boundary (enumeration range too small) would leave a cut face.
    {
        const int sea = vc::TerrainGenerator::kSeaLevel;
        size_t lakeLiquid = 0;
        size_t exposed = 0;
        constexpr int kD4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int wx = lo; wx < hi; ++wx)
            for (int wz = lo; wz < hi; ++wz)
                for (int wy = sea + 1; wy < vc::kWorldHeightBlocks - 1; ++wy) {
                    const vc::BlockId id = blockAt(wx, wy, wz);
                    // Liquid above sea level can only come from a lake.
                    if (id != vc::blocks::Water && id != vc::blocks::Lava) {
                        continue;
                    }
                    ++lakeLiquid;
                    if (blockAt(wx, wy - 1, wz) == vc::blocks::Air) {
                        ++exposed;
                    }
                    for (const auto& d : kD4) {
                        if (blockAt(wx + d[0], wy, wz + d[1]) == vc::blocks::Air) {
                            ++exposed;
                        }
                    }
                }
        Check(lakeLiquid > 0, "lakes generate above sea level");
        Check(exposed == 0, "lake liquid has no exposed air face (sealed + seam-consistent)");
    }

    // Ores (M21, rebased in M25): coal and iron veins generate, iron stays
    // in the lower half (vanilla y0..64; surface is now ~y65), and every ore
    // cell is buried like the stone it replaced — an ore floating in air or
    // water would mean the carve/ore order or the stone-only predicate broke.
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
        Check(ironMaxY < vc::kWorldHeightBlocks / 2 + 8, "iron stays in the lower half");
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

        // M26: lava is a liquid (corner-sampled surface) but renders OPAQUE —
        // its faces land in the opaque stream, not the blended one.
        vc::ChunkSnapshot lavaSnap;
        auto lavaChunk = std::make_shared<vc::Chunk>();
        lavaChunk->Set(8, 8, 8, vc::blocks::Lava);
        lavaSnap.chunks[vc::ChunkSnapshot::Index(1, 1, 1)] = lavaChunk;
        lavaSnap.skyAbove = true;
        const vc::ChunkMesh lavaMesh = vc::ChunkMesher::Build(lavaSnap);
        Check(lavaMesh.vertices.size() == 24 && lavaMesh.transparentVertices.empty(),
              "lone lava block meshes into the opaque stream (not transparent)");

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

    // Torch mesh (M23): vanilla template_torch in the float MODEL stream —
    // two thin crossed slabs (4 outward side planes spanning the cell) plus
    // a top cap at the EXACT flame height (10/16). With float positions the
    // cap lands pixel-true; the old packed format could only quantise it to
    // ninths. Torches emit no cubic geometry now.
    {
        vc::ChunkSnapshot snapshot;
        auto center = std::make_shared<vc::Chunk>();
        center->Set(8, 8, 8, vc::blocks::Torch);
        snapshot.chunks[vc::ChunkSnapshot::Index(1, 1, 1)] = center;
        snapshot.skyAbove = true;

        const vc::ChunkMesh mesh = vc::ChunkMesher::Build(snapshot);
        Check(mesh.vertices.empty(), "torch emits no cubic geometry");
        // Four one-sided side planes (4 verts each) + a four-vert top cap.
        Check(mesh.modelVertices.size() == 20, "lone torch meshes to 4 planes + a cap");

        constexpr float kI7 = 7.0f / 16.0f;
        constexpr float kI9 = 9.0f / 16.0f;
        constexpr float kCapY = 10.0f / 16.0f;
        const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
        const auto inset = [&](float v) { return near(v, kI7) || near(v, kI9); };
        int sidePlaneVerts = 0;
        int capVerts = 0;
        bool geomOk = true;
        for (const auto& vtx : mesh.modelVertices) {
            const float lx = vtx.x - 8.0f, ly = vtx.y - 8.0f, lz = vtx.z - 8.0f;
            const uint32_t normal = (vtx.packed >> 16) & 7u;
            const uint32_t ao = (vtx.packed >> 19) & 15u;
            geomOk &= normal < 6 && ao == 15; // full-bright AO
            if (near(ly, kCapY)) {
                // Cap: both horizontal axes inset to the 2x2 post, raised to
                // the exact flame height.
                ++capVerts;
                geomOk &= inset(lx) && inset(lz);
            } else {
                // Side plane: exactly one axis inset; the other spans the
                // full cell (0 or 1); full height (y in 0..1).
                ++sidePlaneVerts;
                geomOk &= inset(lx) != inset(lz);
                geomOk &= ly >= -1e-4f && ly <= 1.0f + 1e-4f;
            }
        }
        Check(sidePlaneVerts == 16 && capVerts == 4, "torch is 4 side planes + a 4-vert cap");
        Check(geomOk, "torch verts: planes one-axis inset full height, cap both-axis at 10/16");
    }

    // Wall torch (M24): same box count, but the model is oriented (tilted +
    // shoved against the wall it points away from), so it leaves the upright
    // floor-torch footprint. A +X-pointing torch hangs on the -X wall and is
    // shifted toward it (some vertex crosses below the cell's x origin).
    {
        vc::ChunkSnapshot snapshot;
        auto center = std::make_shared<vc::Chunk>();
        center->Set(8, 8, 8, vc::blocks::Torch);
        center->SetMeta(8, 8, 8, vc::facing::TorchWallMeta(vc::BlockFace::PosX));
        snapshot.chunks[vc::ChunkSnapshot::Index(1, 1, 1)] = center;
        snapshot.skyAbove = true;

        const vc::ChunkMesh mesh = vc::ChunkMesher::Build(snapshot);
        Check(mesh.modelVertices.size() == 20, "wall torch still meshes to 4 planes + a cap");
        bool shiftedToWall = false;
        for (const auto& vtx : mesh.modelVertices) {
            shiftedToWall |= vtx.x < 8.0f; // pushed toward the -X wall
        }
        Check(shiftedToWall, "wall torch is oriented (tilted/shifted toward its wall)");
    }

    if (g_failures == 0) {
        std::printf("GenTest: all checks passed (%zu logs, %zu leaves)\n", logs, leafBlocks);
    } else {
        std::printf("GenTest: %d check(s) FAILED\n", g_failures);
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
