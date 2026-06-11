#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "vox/core/ThreadPool.h"
#include "vox/renderer/VertexArray.h"

#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/TerrainGen.h"

namespace vc {

// Lifecycle of a streamed chunk. Generation and meshing run on worker
// threads; every state transition (and the GPU upload) happens on the main
// thread inside World::Update().
enum class ChunkState : uint8_t {
    Generating, // gen job in flight; blocks is still null
    NeedsMesh,  // has block data, waiting on neighbors or the job budget
    Meshing,    // mesh job in flight
    Ready,      // mesh uploaded (mesh stays null for all-air chunks)
};

struct ChunkEntry {
    // Immutable once set: M4 has no block edits, so meshing jobs can share
    // ownership instead of copying. M6 (block break/place) will need
    // copy-on-write or remesh versioning here.
    std::shared_ptr<const Chunk> blocks;
    ChunkState state = ChunkState::Generating;
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
    static constexpr int kViewRadius = 12;  // horizontal streaming radius, in chunks

    explicit World(int seed);

    void Update(const glm::vec3& cameraPos);

    // World-space block query; air for unloaded chunks or outside the world.
    BlockId GetBlock(int wx, int wy, int wz) const;

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
    size_t m_pendingMeshes = 0; // chunks in radius not yet Ready
    size_t m_jobsInFlight = 0;  // main-thread counter: ++submit, --drain

    std::mutex m_completedMutex; // guards the two result queues
    std::vector<GenResult> m_completedGen;
    std::vector<MeshResult> m_completedMesh;

    // Declared last so it is destroyed first: joining the workers before
    // the queues/map/generator they reference go away.
    vox::ThreadPool m_pool;
};

} // namespace vc
