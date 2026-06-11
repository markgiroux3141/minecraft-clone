#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "vox/core/Log.h"

namespace vc {

namespace {

// Cap on generation + meshing jobs in flight, as a multiple of the worker
// count. Keeps the queue short so streaming stays responsive to camera
// movement (work is re-prioritized nearest-first every Update), while
// leaving enough queued that workers never starve between frames.
constexpr size_t kInFlightPerWorker = 4;

int FloorDivChunk(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / static_cast<float>(Chunk::kSize)));
}

} // namespace

World::World(int seed) : m_generator(seed) {}

BlockId World::GetBlock(int wx, int wy, int wz) const {
    // Arithmetic shift floors negative coordinates correctly for power-of-two sizes.
    const glm::ivec3 coord{wx >> 4, wy >> 4, wz >> 4};
    const auto it = m_chunks.find(coord);
    if (it == m_chunks.end() || !it->second.blocks) {
        return blocks::Air;
    }
    return it->second.blocks->Get(wx & 15, wy & 15, wz & 15);
}

const Chunk* World::GetChunk(const glm::ivec3& chunkCoord) const {
    const auto it = m_chunks.find(chunkCoord);
    return it != m_chunks.end() ? it->second.blocks.get() : nullptr;
}

// AO samples diagonal blocks, so meshing waits on all 26 neighbors (the
// streaming data ring is radius+1, so they always arrive eventually).
bool World::NeighborsHaveData(const glm::ivec3& coord) const {
    for (int dy = -1; dy <= 1; ++dy) {
        const int ny = coord.y + dy;
        if (ny < 0 || ny >= kHeightChunks) {
            continue; // outside the world counts as air, no data needed
        }
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0 && dz == 0) {
                    continue;
                }
                const auto it = m_chunks.find(coord + glm::ivec3{dx, dy, dz});
                if (it == m_chunks.end() || !it->second.blocks) {
                    return false;
                }
            }
        }
    }
    return true;
}

ChunkSnapshot World::SnapshotFor(const glm::ivec3& coord) const {
    ChunkSnapshot snapshot;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto it = m_chunks.find(coord + glm::ivec3{dx, dy, dz});
                if (it != m_chunks.end()) {
                    snapshot.chunks[ChunkSnapshot::Index(dx + 1, dy + 1, dz + 1)] =
                        it->second.blocks;
                }
            }
        }
    }
    return snapshot;
}

void World::SubmitGenerate(const glm::ivec3& coord) {
    m_chunks.emplace(coord, ChunkEntry{}); // state defaults to Generating
    ++m_jobsInFlight;
    m_pool.Submit([this, coord] {
        auto chunk = std::make_shared<Chunk>();
        m_generator.Generate(*chunk, coord);
        std::lock_guard lock(m_completedMutex);
        m_completedGen.push_back({coord, std::move(chunk)});
    });
}

void World::SubmitMesh(const glm::ivec3& coord) {
    ChunkEntry& entry = m_chunks.at(coord);
    entry.state = ChunkState::Meshing;
    ++m_jobsInFlight;
    m_pool.Submit([this, coord, snapshot = SnapshotFor(coord)] {
        MeshResult result{coord, ChunkMesher::Build(snapshot)};
        std::lock_guard lock(m_completedMutex);
        m_completedMesh.push_back(std::move(result));
    });
}

void World::UploadMesh(ChunkEntry& entry, const ChunkMesh& mesh) {
    entry.state = ChunkState::Ready;

    if (mesh.vertices.empty()) {
        entry.mesh = nullptr;
        entry.indexCount = 0;
        return;
    }

    auto vertexBuffer = std::make_shared<vox::VertexBuffer>(
        mesh.vertices.data(),
        static_cast<uint32_t>(mesh.vertices.size() * sizeof(ChunkVertex)));
    vertexBuffer->SetLayout({
        {vox::ShaderDataType::Float3, "a_position"},
        {vox::ShaderDataType::Float3, "a_normal"},
        {vox::ShaderDataType::Float2, "a_uv"},
        {vox::ShaderDataType::Float, "a_layer"},
        {vox::ShaderDataType::Float, "a_ao"},
    });
    auto indexBuffer = std::make_shared<vox::IndexBuffer>(
        mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size()));

    entry.mesh = std::make_shared<vox::VertexArray>();
    entry.mesh->AddVertexBuffer(std::move(vertexBuffer));
    entry.mesh->SetIndexBuffer(std::move(indexBuffer));
    entry.indexCount = static_cast<uint32_t>(mesh.indices.size());
}

void World::DrainCompletedJobs() {
    std::vector<GenResult> gens;
    std::vector<MeshResult> meshes;
    {
        std::lock_guard lock(m_completedMutex);
        gens.swap(m_completedGen);
        meshes.swap(m_completedMesh);
    }
    m_jobsInFlight -= gens.size() + meshes.size();

    // The state checks drop results for chunks that were unloaded (or
    // unloaded and re-requested) while the job was in flight. Accepting a
    // stale result for a re-requested coord is fine today because the
    // world is deterministic and immutable; M6 edits will need versioning.
    for (auto& gen : gens) {
        const auto it = m_chunks.find(gen.coord);
        if (it == m_chunks.end() || it->second.state != ChunkState::Generating) {
            continue;
        }
        it->second.blocks = std::move(gen.blocks);
        it->second.state = ChunkState::NeedsMesh;
    }
    for (auto& meshed : meshes) {
        const auto it = m_chunks.find(meshed.coord);
        if (it == m_chunks.end() || it->second.state != ChunkState::Meshing) {
            continue;
        }
        UploadMesh(it->second, meshed.mesh);
    }
}

void World::Update(const glm::vec3& cameraPos) {
    DrainCompletedJobs();

    const int centerX = FloorDivChunk(cameraPos.x);
    const int centerZ = FloorDivChunk(cameraPos.z);

    // Unload chunks outside the data radius (view + 1 ring of neighbor
    // data). In-flight jobs for erased coords resolve harmlessly: their
    // results are dropped in DrainCompletedJobs, and snapshots keep the
    // block data itself alive until the job finishes.
    std::vector<glm::ivec3> toErase;
    for (const auto& [coord, entry] : m_chunks) {
        const int dist = std::max(std::abs(coord.x - centerX), std::abs(coord.z - centerZ));
        if (dist > kViewRadius + 1) {
            toErase.push_back(coord);
        }
    }
    for (const auto& coord : toErase) {
        m_chunks.erase(coord);
    }

    // Collect missing work, nearest column first.
    std::vector<std::pair<int, glm::ivec3>> toGenerate;
    std::vector<std::pair<int, glm::ivec3>> toMesh;
    m_pendingMeshes = 0;
    for (int dz = -kViewRadius - 1; dz <= kViewRadius + 1; ++dz) {
        for (int dx = -kViewRadius - 1; dx <= kViewRadius + 1; ++dx) {
            const int dist2 = dx * dx + dz * dz;
            const bool inViewRadius =
                std::max(std::abs(dx), std::abs(dz)) <= kViewRadius;
            for (int cy = 0; cy < kHeightChunks; ++cy) {
                const glm::ivec3 coord{centerX + dx, cy, centerZ + dz};
                const auto it = m_chunks.find(coord);
                if (it == m_chunks.end()) {
                    toGenerate.emplace_back(dist2, coord);
                    if (inViewRadius) {
                        ++m_pendingMeshes;
                    }
                    continue;
                }
                const ChunkState state = it->second.state;
                if (inViewRadius && state != ChunkState::Ready) {
                    ++m_pendingMeshes;
                    if (state == ChunkState::NeedsMesh && NeighborsHaveData(coord)) {
                        toMesh.emplace_back(dist2, coord);
                    }
                }
            }
        }
    }

    auto byDistance = [](const auto& a, const auto& b) { return a.first < b.first; };
    std::sort(toGenerate.begin(), toGenerate.end(), byDistance);
    std::sort(toMesh.begin(), toMesh.end(), byDistance);

    // Meshes first: they turn into visible geometry immediately, while
    // generation only feeds future meshes.
    const size_t maxInFlight = m_pool.ThreadCount() * kInFlightPerWorker;
    for (const auto& [dist2, coord] : toMesh) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitMesh(coord);
    }
    for (const auto& [dist2, coord] : toGenerate) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitGenerate(coord);
    }
}

} // namespace vc
