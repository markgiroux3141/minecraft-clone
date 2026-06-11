#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "vox/core/ThreadPool.h"
#include "vox/renderer/MeshPool.h"

#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/Light.h"
#include "world/LightEngine.h"
#include "world/TerrainGen.h"
#include "world/WorldSave.h"

namespace vc {

// Per-chunk record. Block and light data are copy-on-write: the pointers
// are replaced (never mutated), so worker-thread snapshots stay valid
// without locks. Mesh freshness is tracked with versions — in-flight
// results are matched against dataVersion and dropped when stale.
struct ChunkEntry {
    std::shared_ptr<const Chunk> blocks;     // null while the gen job is in flight
    std::shared_ptr<const ChunkLight> light; // null until the column is first lit
    uint32_t dataVersion = 0;   // mesh-input version: bumped by edits AND light changes
    uint32_t meshedVersion = 0; // dataVersion the uploaded mesh was built from
    uint32_t meshingVersion = 0; // dataVersion the in-flight mesh job targets (0 = none)
    // Allocation in World's MeshPool; invalid until uploaded, or if empty.
    vox::MeshPool::MeshHandle mesh = vox::MeshPool::kInvalidMesh;
    uint32_t indexCount = 0;
    bool edited = false; // diverges from the save store — persist before unload
};

// Per-column light bookkeeping (light is computed column-at-a-time because
// sky light depends on the whole column above). Same versioning pattern as
// chunk meshes; the resulting ChunkLight sections live on the ChunkEntries.
struct ColumnEntry {
    uint32_t dirtySeq = 1;    // bumped when blocks change within light range
    uint32_t litSeq = 0;      // dirtySeq the stored light was computed at
    uint32_t lightingSeq = 0; // dirtySeq the in-flight light job targets (0 = none)
};

// Owns all loaded chunks and streams them around the camera. Workers
// produce CPU-side data (generated chunks, column light, chunk meshes) and
// push it to completion queues; Update() drains those queues, mutates the
// maps, and uploads meshes to the GPU — the maps and GL are
// main-thread-only.
//
// Radius layering (each stage needs a 3x3 neighborhood of the previous):
// generate at kViewRadius+2, light at kViewRadius+1, mesh at kViewRadius.
class World {
public:
    static constexpr int kHeightChunks = kWorldHeightChunks; // world height: 64 blocks
    static constexpr int kHeightBlocks = kWorldHeightBlocks;
    static constexpr int kViewRadius = 12; // horizontal mesh radius, in chunks

    // defaultSeed only applies to a brand-new save; an existing save's
    // manifest seed wins so its untouched chunks regenerate identically.
    World(int defaultSeed, std::filesystem::path saveDir);
    ~World(); // saves all edited chunks and force-flushes the store

    void Update(const glm::vec3& cameraPos);

    // World-space block query; air for unloaded chunks or outside the world.
    BlockId GetBlock(int wx, int wy, int wz) const;

    bool IsSolid(int wx, int wy, int wz) const;

    // Copy-on-write block edit. Marks the chunk (and, for border blocks,
    // the affected neighbors) for remeshing and dirties every column whose
    // light the edit can reach. No-op outside the world or in ungenerated
    // chunks.
    void SetBlock(const glm::ivec3& worldPos, BlockId id);

    struct RaycastHit {
        glm::ivec3 block;
        glm::ivec3 normal; // unit axis vector of the face that was hit
    };

    // Amanatides & Woo voxel walk to the first solid block. Returns nothing
    // if the ray starts inside a solid block or exhausts maxDistance.
    std::optional<RaycastHit> RaycastBlocks(const glm::vec3& origin, const glm::vec3& dir,
                                            float maxDistance) const;

    const Chunk* GetChunk(const glm::ivec3& chunkCoord) const;

    template <typename Fn> // fn(chunkCoord, meshHandle, indexCount)
    void ForEachRenderableChunk(Fn&& fn) const {
        for (const auto& [coord, entry] : m_chunks) {
            if (entry.mesh != vox::MeshPool::kInvalidMesh) {
                fn(coord, entry.mesh, entry.indexCount);
            }
        }
    }

    // All chunk meshes live here; the renderer draws them in one
    // glMultiDrawElementsIndirect via Meshes().Draw(items).
    vox::MeshPool& Meshes() { return m_meshPool; }

    size_t LoadedChunkCount() const { return m_chunks.size(); }
    size_t PendingMeshCount() const { return m_pendingMeshes; }
    size_t JobsInFlight() const { return m_jobsInFlight; }

private:
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const {
            // Large-prime mix; fine for chunk coordinates.
            return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
                   static_cast<size_t>(v.z) * 83492791u;
        }
    };
    struct IVec2Hash {
        size_t operator()(const glm::ivec2& v) const {
            return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 83492791u;
        }
    };
    using ChunkMap = std::unordered_map<glm::ivec3, ChunkEntry, IVec3Hash>;
    using ColumnMap = std::unordered_map<glm::ivec2, ColumnEntry, IVec2Hash>;

    struct GenResult {
        glm::ivec3 coord;
        std::shared_ptr<const Chunk> blocks;
    };
    struct LightResult {
        glm::ivec2 column;
        uint32_t version; // ColumnEntry::dirtySeq the input was captured at
        std::array<std::shared_ptr<const ChunkLight>, kWorldHeightChunks> light;
    };
    struct MeshResult {
        glm::ivec3 coord;
        uint32_t version; // dataVersion the snapshot was taken at
        ChunkMesh mesh;
    };

    void DrainCompletedJobs();
    // Packed light at a world position (sky 15 above the world, 0 when
    // unloaded/below).
    uint8_t PackedLightAt(const glm::ivec3& worldPos) const;
    void SubmitGenerate(const glm::ivec3& coord);
    void SubmitLight(const glm::ivec2& column);
    void SubmitMesh(const glm::ivec3& coord);
    void UploadMesh(ChunkEntry& entry, const ChunkMesh& mesh);
    ChunkSnapshot SnapshotFor(const glm::ivec3& coord) const;
    // Mesh gating: all 26 neighbors have blocks, and the whole 3x3x3
    // neighborhood (center included) has light.
    bool NeighborsReady(const glm::ivec3& coord) const;
    // Light gating: the 3x3 column neighborhood is fully generated.
    bool ColumnHasData(const glm::ivec2& column) const;
    // Put()s every loaded edited chunk into the save store (autosave/quit).
    void SaveEditedChunks();

    WorldSave m_save; // declared before m_generator: its manifest provides the seed
    TerrainGenerator m_generator; // stateless — shared by all workers
    vox::MeshPool m_meshPool;     // main-thread-only, like all GL
    ChunkMap m_chunks;
    ColumnMap m_columns;
    size_t m_pendingMeshes = 0; // chunks in radius without an up-to-date mesh
    size_t m_jobsInFlight = 0;  // main-thread counter: ++submit, --drain
    std::chrono::steady_clock::time_point m_lastAutosave;

    std::mutex m_completedMutex; // guards the result queues
    std::vector<GenResult> m_completedGen;
    std::vector<LightResult> m_completedLight;
    std::vector<MeshResult> m_completedMesh;

    // Declared last so it is destroyed first: joining the workers before
    // the queues/maps/generator they reference go away.
    vox::ThreadPool m_pool;
};

} // namespace vc
