#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "world/Chunk.h"

namespace vc {

// On-disk persistence for player-edited chunks. Only chunks that diverge
// from the generator are stored; everything else regenerates from the seed.
//
// Layout (all fields little-endian):
//   <dir>/level.dat           text manifest: save format version + seed,
//                             plus optional tagged lines in any order:
//                             "player x y z yaw pitch fly" (M10), "time t"
//                             (M11), "inventory n {slot id count}*n" (M17)
//   <dir>/r.<rx>.<rz>.vxr     region file covering 32x32 chunk columns:
//     uint32 magic "VXR1", uint32 chunkCount,
//     chunkCount x { int32 cx, cy, cz; uint32 blobSize },
//     then the chunk blobs concatenated in index order.
//
// Chunk blob: one format byte — 0 = RLE pairs of uint16 {id, runLength}
// over the YZX-ordered 16^3 ids, 1 = raw uint16 ids (used when RLE would
// be larger) — followed by the payload.
//
// The whole store loads into memory at construction (edited chunks are
// sparse and RLE keeps blobs tiny) and is main-thread-only, like the chunk
// map: workers receive blob copies and decode them off-thread. Flush
// rewrites dirty region files atomically (temp file + rename) and is
// debounced unless forced.
class WorldSave {
public:
    static constexpr int kRegionSize = 32; // chunk columns per region axis

    // Creates the directory if needed; reads the manifest (writing one
    // with defaultSeed if absent) and every region file found.
    WorldSave(std::filesystem::path dir, int defaultSeed);

    int Seed() const { return m_seed; }

    // Where the player left off, stored in the manifest. Absent in saves
    // that predate it (the game falls back to the default spawn).
    struct PlayerState {
        glm::vec3 position{0.0f}; // feet center
        float yaw = 0.0f;
        float pitch = 0.0f;
        bool fly = false;
    };
    const std::optional<PlayerState>& GetPlayerState() const { return m_player; }
    // Rewrites the manifest immediately (it's a quit-path write, not per-frame).
    void SetPlayerState(const PlayerState& state);

    // World time in ticks (day/night cycle), also in the manifest; absent
    // in saves that predate it.
    const std::optional<int64_t>& GetWorldTime() const { return m_worldTime; }
    void SetWorldTime(int64_t ticks);

    // Player inventory (M17), also in the manifest: non-empty slots only,
    // one "inventory <n> <slot> <id> <count>..." line. Absent in saves
    // that predate it (the game grants the legacy starter hotbar); an
    // empty inventory persists as "inventory 0".
    struct InventorySlot {
        int slot = 0;
        uint16_t id = 0;
        int count = 0;
    };
    const std::optional<std::vector<InventorySlot>>& GetInventory() const { return m_inventory; }
    // Rewrites the manifest immediately, like SetPlayerState (quit-path).
    void SetInventory(std::vector<InventorySlot> slots);

    // Null when the chunk was never saved. Invalidated by the next Put.
    const std::vector<uint8_t>* FindBlob(const glm::ivec3& chunkCoord) const;

    // False (and out reset to all air) when the blob is malformed.
    static bool Decode(const std::vector<uint8_t>& blob, Chunk& out);

    // Stores (or replaces) the chunk's blob and marks its region dirty.
    void Put(const glm::ivec3& chunkCoord, const Chunk& chunk);

    // Writes dirty regions to disk; debounced to one pass every few
    // seconds unless force is set (the quit path).
    void Flush(bool force);

    size_t SavedChunkCount() const { return m_count; }

private:
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const {
            return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
                   static_cast<size_t>(v.z) * 83492791u;
        }
    };
    struct IVec2Hash {
        size_t operator()(const glm::ivec2& v) const {
            return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 83492791u;
        }
    };
    using RegionChunks = std::unordered_map<glm::ivec3, std::vector<uint8_t>, IVec3Hash>;

    static glm::ivec2 RegionOf(const glm::ivec3& chunkCoord) {
        // Arithmetic shift floors negative coordinates (kRegionSize = 2^5).
        return {chunkCoord.x >> 5, chunkCoord.z >> 5};
    }

    void ReadManifest(int defaultSeed);
    void WriteManifest() const;
    void ReadRegionFile(const std::filesystem::path& path);
    void WriteRegionFile(const glm::ivec2& region) const;

    std::filesystem::path m_dir;
    int m_seed = 0;
    std::optional<PlayerState> m_player;
    std::optional<int64_t> m_worldTime;
    std::optional<std::vector<InventorySlot>> m_inventory;
    std::unordered_map<glm::ivec2, RegionChunks, IVec2Hash> m_regions;
    std::unordered_set<glm::ivec2, IVec2Hash> m_dirtyRegions;
    size_t m_count = 0;
    std::chrono::steady_clock::time_point m_lastFlush;
};

} // namespace vc
