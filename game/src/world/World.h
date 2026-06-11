#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "vox/core/ThreadPool.h"
#include "vox/renderer/VertexArray.h"

#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/TerrainGen.h"

namespace vc {

// Per-chunk record. Block data is copy-on-write: `blocks` is replaced (never
// mutated) on edit, so worker-thread snapshots stay valid without locks.
// Mesh freshness is tracked with versions instead of a state machine —
// in-flight results are matched against dataVersion and dropped when stale.
struct ChunkEntry {
    std::shared_ptr<const Chunk> blocks; // null while the gen job is in flight
    uint32_t dataVersion = 0;            // 0 = no data yet; bumped on every edit
    uint32_t meshedVersion = 0;          // dataVersion the uploaded mesh was built from
    uint32_t meshingVersion = 0;         // dataVersion the in-flight mesh job targets (0 = none)
    std::shared_ptr<vox::VertexArray> mesh; // null until uploaded, or if empty
    uint32_t indexCount = 0;
};

// Owns all loaded chunks and streams them around the camera. Workers
// produce CPU-side data (generated chunks, chunk meshes) and push it to
// completion queues; Update() drains those queues, mutates the chunk map,
// and uploads meshes to the GPU — the map and GL are main-thread-only.
class World {
public:
    static constexpr int kHeightChunks = 4; // world height: 64 blocks
    static constexpr int kHeightBlocks = kHeightChunks * Chunk::kSize;
    static constexpr int kViewRadius = 12; // horizontal streaming radius, in chunks

    explicit World(int seed);

    void Update(const glm::vec3& cameraPos);

    // World-space block query; air for unloaded chunks or outside the world.
    BlockId GetBlock(int wx, int wy, int wz) const;

    bool IsSolid(int wx, int wy, int wz) const;

    // Copy-on-write block edit. Marks the chunk (and, for border blocks,
    // the affected neighbors — face culling and AO reach across the seam)
    // for remeshing. No-op outside the world or in ungenerated chunks.
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

    template <typename Fn> // fn(chunkCoord, vertexArray, indexCount)
    void ForEachRenderableChunk(Fn&& fn) const {
        for (const auto& [coord, entry] : m_chunks) {
            if (entry.mesh) {
                fn(coord, *entry.mesh, entry.indexCount);
            }
        }
    }

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
    using ChunkMap = std::unordered_map<glm::ivec3, ChunkEntry, IVec3Hash>;

    struct GenResult {
        glm::ivec3 coord;
        std::shared_ptr<const Chunk> blocks;
    };
    struct MeshResult {
        glm::ivec3 coord;
        uint32_t version; // dataVersion the snapshot was taken at
        ChunkMesh mesh;
    };

    void DrainCompletedJobs();
    void SubmitGenerate(const glm::ivec3& coord);
    void SubmitMesh(const glm::ivec3& coord);
    void UploadMesh(ChunkEntry& entry, const ChunkMesh& mesh);
    ChunkSnapshot SnapshotFor(const glm::ivec3& coord) const;
    bool NeighborsHaveData(const glm::ivec3& coord) const;

    TerrainGenerator m_generator; // stateless — shared by all workers
    ChunkMap m_chunks;
    size_t m_pendingMeshes = 0; // chunks in radius without an up-to-date mesh
    size_t m_jobsInFlight = 0;  // main-thread counter: ++submit, --drain

    std::mutex m_completedMutex; // guards the two result queues
    std::vector<GenResult> m_completedGen;
    std::vector<MeshResult> m_completedMesh;

    // Declared last so it is destroyed first: joining the workers before
    // the queues/map/generator they reference go away.
    vox::ThreadPool m_pool;
};

} // namespace vc
