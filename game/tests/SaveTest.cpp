// Round-trip regression test for WorldSave: encode/decode, region file
// write/read, manifest seed, and overwrite-on-re-put. Run after changing
// the save format. Exits 0 on pass, 1 on the first failure.

#include <cstdio>
#include <cstdlib>
#include <filesystem>

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
        save.SetPlayerState({{1234.5f, 70.25f, -8.125f}, 123.5f, -45.0f, true});
    }
    {
        vc::WorldSave save(dir, 7);
        const auto& player = save.GetPlayerState();
        Check(player.has_value(), "player state survives a reload");
        Check(player && player->position == glm::vec3(1234.5f, 70.25f, -8.125f) &&
                  player->yaw == 123.5f && player->pitch == -45.0f && player->fly,
              "player state round-trips exactly");
        Check(save.Seed() == 42, "seed survives the manifest rewrite");
        Check(save.SavedChunkCount() == 4, "chunks survive the manifest rewrite");
    }

    std::filesystem::remove_all(dir);
    std::printf(g_failures == 0 ? "SaveTest: all checks passed\n"
                                : "SaveTest: %d check(s) FAILED\n",
                g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
