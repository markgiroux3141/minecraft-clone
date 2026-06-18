// Round-trip regression test for WorldSave: encode/decode, region file
// write/read, manifest seed, and overwrite-on-re-put. Run after changing
// the save format. Exits 0 on pass, 1 on the first failure.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "vox/core/Log.h"

#include "world/WorldSave.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "  ok " : "FAIL ", what);
    if (!ok) {
        ++g_failures;
    }
}

// Deterministic non-trivial pattern: terrain-like runs plus scattered ids,
// so both the RLE path and multi-run chunks get exercised.
vc::Chunk MakeChunk(int salt) {
    vc::Chunk chunk;
    for (int y = 0; y < vc::Chunk::kSize; ++y) {
        for (int z = 0; z < vc::Chunk::kSize; ++z) {
            for (int x = 0; x < vc::Chunk::kSize; ++x) {
                vc::BlockId id = y < 8 ? 1 : 0;
                if ((x * 7 + y * 13 + z * 31 + salt) % 97 == 0) {
                    id = static_cast<vc::BlockId>(2 + (x + salt) % 3);
                }
                chunk.Set(x, y, z, id);
            }
        }
    }
    return chunk;
}

bool SameBlocks(const vc::Chunk& a, const vc::Chunk& b) {
    return a.Raw() == b.Raw();
}

// MakeChunk plus a few oriented cells (M24): exercises the meta RLE stream
// and the format bump.
vc::Chunk MakeMetaChunk(int salt) {
    vc::Chunk chunk = MakeChunk(salt);
    chunk.SetMeta(0, 0, 0, 5);
    chunk.SetMeta(8, 4, 2, 1);
    chunk.SetMeta(15, 15, 15, 3);
    return chunk;
}

} // namespace

int main() {
    vox::Log::Init();
    const auto dir = std::filesystem::temp_directory_path() / "voxcraft-savetest";
    std::filesystem::remove_all(dir);

    // Coordinates spanning two regions, including negative region coords.
    const glm::ivec3 coords[] = {{0, 0, 0}, {31, 3, 31}, {-1, 2, -33}, {5, 1, 5}};
    {
        vc::WorldSave save(dir, 42);
        Check(save.Seed() == 42, "new save adopts the default seed");
        for (int i = 0; i < 4; ++i) {
            save.Put(coords[i], MakeChunk(i));
        }
        Check(save.SavedChunkCount() == 4, "four chunks stored");
        save.Flush(true);
    }
    {
        vc::WorldSave save(dir, 7); // wrong default: the manifest must win
        Check(save.Seed() == 42, "manifest seed wins over the default");
        Check(save.SavedChunkCount() == 4, "all chunks reload from region files");
        for (int i = 0; i < 4; ++i) {
            const auto* blob = save.FindBlob(coords[i]);
            vc::Chunk decoded;
            Check(blob && vc::WorldSave::Decode(*blob, decoded) &&
                      SameBlocks(decoded, MakeChunk(i)),
                  "chunk round-trips bit-exact");
        }
        Check(save.FindBlob({99, 0, 99}) == nullptr, "unsaved coord stays null");
        save.Put(coords[0], MakeChunk(50)); // overwrite, then persist
        Check(save.SavedChunkCount() == 4, "re-put replaces instead of duplicating");
        save.Flush(true);
    }
    {
        vc::WorldSave save(dir, 7);
        const auto* blob = save.FindBlob(coords[0]);
        vc::Chunk decoded;
        Check(blob && vc::WorldSave::Decode(*blob, decoded) &&
                  SameBlocks(decoded, MakeChunk(50)),
              "overwritten chunk reloads with the new contents");
        vc::Chunk garbage;
        Check(!vc::WorldSave::Decode({0, 1, 2, 3}, garbage), "corrupt blob rejected");

        Check(!save.GetPlayerState().has_value(), "pre-player-state manifest reads as absent");
        Check(!save.GetWorldTime().has_value(), "pre-time manifest reads as absent");
        Check(!save.GetInventory().has_value(), "pre-inventory manifest reads as absent");
        Check(!save.GetVitals().has_value(), "pre-vitals manifest reads as absent");
        Check(!save.GetArmor().has_value(), "pre-armor manifest reads as absent");
        save.SetWorldTime(13337);
        save.SetPlayerState({{1234.5f, 70.25f, -8.125f}, 123.5f, -45.0f, true});
        save.SetInventory({{0, 1, 64, 0}, {8, 1025, 1, 37}, {35, 20, 17, 0}});
        save.SetVitals({7.5f, 13, 2.25f, 1.5f, 140});
        // M33 armor: helmet (slot 0) + boots (slot 3), worn (count 1) with wear.
        save.SetArmor({{0, 1095, 1, 12}, {3, 1098, 1, 0}});
    }
    {
        vc::WorldSave save(dir, 7);
        const auto& player = save.GetPlayerState();
        Check(player.has_value(), "player state survives a reload");
        Check(player && player->position == glm::vec3(1234.5f, 70.25f, -8.125f) &&
                  player->yaw == 123.5f && player->pitch == -45.0f && player->fly,
              "player state round-trips exactly");
        Check(save.GetWorldTime() == 13337, "world time round-trips");
        const auto& inv = save.GetInventory();
        Check(inv.has_value() && inv->size() == 3 && (*inv)[0].slot == 0 && (*inv)[0].id == 1 &&
                  (*inv)[0].count == 64 && (*inv)[0].damage == 0 && (*inv)[1].slot == 8 &&
                  (*inv)[1].id == 1025 && (*inv)[1].count == 1 && (*inv)[1].damage == 37 &&
                  (*inv)[2].slot == 35 && (*inv)[2].id == 20 && (*inv)[2].count == 17,
              "inventory (incl. item ids + tool damage) round-trips exactly");
        const auto& vitals = save.GetVitals();
        Check(vitals.has_value() && vitals->health == 7.5f && vitals->foodLevel == 13 &&
                  vitals->saturation == 2.25f && vitals->exhaustion == 1.5f && vitals->air == 140,
              "vitals (health/food/saturation/exhaustion/air) round-trip exactly");
        const auto& armor = save.GetArmor();
        Check(armor.has_value() && armor->size() == 2 && (*armor)[0].slot == 0 &&
                  (*armor)[0].id == 1095 && (*armor)[0].count == 1 && (*armor)[0].damage == 12 &&
                  (*armor)[1].slot == 3 && (*armor)[1].id == 1098 && (*armor)[1].damage == 0,
              "worn armor (slot/id/count/wear) round-trips exactly");
        Check(save.Seed() == 42, "seed survives the manifest rewrite");
        Check(save.SavedChunkCount() == 4, "chunks survive the manifest rewrite");
        save.SetInventory({});
    }
    {
        vc::WorldSave save(dir, 7);
        const auto& inv = save.GetInventory();
        Check(inv.has_value() && inv->empty(),
              "empty inventory reads back as present-but-empty (not absent)");
        Check(save.GetArmor().has_value() && save.GetArmor()->size() == 2,
              "armor persists across an inventory-only rewrite");
    }
    {
        // Furnace block entities (M21): the furnaces.dat sidecar
        // round-trips slots (incl. damage), burn, and cook progress;
        // an empty set removes the file.
        vc::WorldSave save(dir, 7);
        Check(save.GetFurnaces().empty(), "pre-furnace save reads as no furnaces");
        vc::WorldSave::FurnaceRecord a;
        a.pos = {12, 5, -7};
        a.id = {51, 1025, 52};
        a.count = {3, 1, 2};
        a.damage = {0, 37, 0};
        a.burnTicks = 1234;
        a.burnTotal = 1600;
        a.cookTicks = 150;
        vc::WorldSave::FurnaceRecord b;
        b.pos = {-3, 0, 4};
        save.SetFurnaces({a, b});
        save.Flush(true);
    }
    {
        vc::WorldSave save(dir, 7);
        const auto& furnaces = save.GetFurnaces();
        Check(furnaces.size() == 2, "furnaces reload from the sidecar");
        Check(furnaces.size() == 2 && furnaces[0].pos == glm::ivec3(12, 5, -7) &&
                  furnaces[0].id[0] == 51 && furnaces[0].count[0] == 3 &&
                  furnaces[0].id[1] == 1025 && furnaces[0].damage[1] == 37 &&
                  furnaces[0].id[2] == 52 && furnaces[0].count[2] == 2 &&
                  furnaces[0].burnTicks == 1234 && furnaces[0].burnTotal == 1600 &&
                  furnaces[0].cookTicks == 150,
              "furnace slots/progress round-trip exactly");
        Check(furnaces.size() == 2 && furnaces[1].pos == glm::ivec3(-3, 0, 4) &&
                  furnaces[1].count[0] == 0 && furnaces[1].burnTicks == 0,
              "empty furnace round-trips");
        save.SetFurnaces({});
        save.Flush(true);
    }
    {
        vc::WorldSave save(dir, 7);
        Check(save.GetFurnaces().empty(), "clearing furnaces removes them");
        Check(!std::filesystem::exists(dir / "furnaces.dat"),
              "empty furnace set deletes the sidecar");
    }
    {
        // Mobs (M32): the mobs.dat sidecar round-trips type/pos/yaw/health;
        // an empty set removes the file.
        vc::WorldSave save(dir, 7);
        Check(save.GetMobs().empty(), "pre-mob save reads as no mobs");
        vc::WorldSave::MobRecord a{1, {10.5f, 68.0f, -4.25f}, 1.57f, 18.0f}; // zombie
        vc::WorldSave::MobRecord b{0, {-30.0f, 70.0f, 12.0f}, -0.5f, 10.0f}; // pig
        vc::WorldSave::MobRecord c{3, {4.0f, 66.0f, 9.0f}, 2.0f, 8.0f};      // M34 sheep (type 3)
        save.SetMobs({a, b, c});
        save.Flush(true);
    }
    {
        vc::WorldSave save(dir, 7);
        const auto& mobs = save.GetMobs();
        Check(mobs.size() == 3, "mobs reload from the sidecar");
        Check(mobs.size() == 3 && mobs[0].type == 1 && mobs[0].pos.x == 10.5f &&
                  mobs[0].pos.y == 68.0f && mobs[0].pos.z == -4.25f && mobs[0].yaw == 1.57f &&
                  mobs[0].health == 18.0f,
              "zombie record round-trips exactly");
        Check(mobs.size() == 3 && mobs[1].type == 0 && mobs[1].pos.x == -30.0f &&
                  mobs[1].health == 10.0f,
              "pig record round-trips");
        Check(mobs.size() == 3 && mobs[2].type == 3 && mobs[2].pos.z == 9.0f &&
                  mobs[2].health == 8.0f,
              "M34 sheep record round-trips");
        save.SetMobs({});
        save.Flush(true);
    }
    {
        vc::WorldSave save(dir, 7);
        Check(save.GetMobs().empty(), "clearing mobs removes them");
        Check(!std::filesystem::exists(dir / "mobs.dat"), "empty mob set deletes the sidecar");
    }
    {
        // M17/M18 saves wrote triples under the old "inventory" tag —
        // they must still parse (damage 0).
        std::ofstream out{dir / "level.dat"};
        out << "voxcraft-save 1\nseed 42\ninventory 1 3 7 12\n";
        out.close();
        vc::WorldSave save(dir, 7);
        const auto& inv = save.GetInventory();
        Check(inv.has_value() && inv->size() == 1 && (*inv)[0].slot == 3 && (*inv)[0].id == 7 &&
                  (*inv)[0].count == 12 && (*inv)[0].damage == 0,
              "legacy triple-form inventory tag still parses");
    }
    {
        // M24: orientation meta round-trips, an unoriented chunk still
        // encodes to the legacy (meta-less) format, and an oriented one uses
        // the new format.
        const std::filesystem::path mdir = dir / "meta";
        std::filesystem::remove_all(mdir);
        const glm::ivec3 plain{1, 0, 1};
        const glm::ivec3 oriented{2, 1, -40};
        {
            vc::WorldSave save(mdir, 9);
            save.Put(plain, MakeChunk(3));
            save.Put(oriented, MakeMetaChunk(4));
            save.Flush(true);
        }
        vc::WorldSave save(mdir, 9);
        const auto* plainBlob = save.FindBlob(plain);
        const auto* metaBlob = save.FindBlob(oriented);
        Check(plainBlob && !plainBlob->empty() && ((*plainBlob)[0] == 0 || (*plainBlob)[0] == 1),
              "unoriented chunk keeps the legacy meta-less format");
        Check(metaBlob && !metaBlob->empty() && ((*metaBlob)[0] == 2 || (*metaBlob)[0] == 3),
              "oriented chunk uses the meta-bearing format");
        vc::Chunk decodedPlain;
        vc::Chunk decodedMeta;
        Check(plainBlob && vc::WorldSave::Decode(*plainBlob, decodedPlain) &&
                  SameBlocks(decodedPlain, MakeChunk(3)) &&
                  decodedPlain.RawMeta() == vc::Chunk{}.RawMeta(),
              "legacy blob decodes to all-zero meta");
        Check(metaBlob && vc::WorldSave::Decode(*metaBlob, decodedMeta) &&
                  SameBlocks(decodedMeta, MakeMetaChunk(4)) &&
                  decodedMeta.RawMeta() == MakeMetaChunk(4).RawMeta(),
              "oriented chunk round-trips ids AND meta exactly");
    }

    std::filesystem::remove_all(dir);
    std::printf(g_failures == 0 ? "SaveTest: all checks passed\n"
                                : "SaveTest: %d check(s) FAILED\n",
                g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
