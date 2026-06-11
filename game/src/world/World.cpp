#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <limits>
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

bool World::IsSolid(int wx, int wy, int wz) const {
    return BlockRegistry::Get().Def(GetBlock(wx, wy, wz)).solid;
}

void World::SetBlock(const glm::ivec3& worldPos, BlockId id) {
    if (worldPos.y < 0 || worldPos.y >= kHeightBlocks) {
        return;
    }
    const glm::ivec3 cc{worldPos.x >> 4, worldPos.y >> 4, worldPos.z >> 4};
    const auto it = m_chunks.find(cc);
    if (it == m_chunks.end() || !it->second.blocks) {
        return; // can't edit what isn't generated
    }
    ChunkEntry& entry = it->second;
    const glm::ivec3 local{worldPos.x & 15, worldPos.y & 15, worldPos.z & 15};
    if (entry.blocks->Get(local.x, local.y, local.z) == id) {
        return;
    }

    // Copy-on-write: in-flight snapshots keep the old chunk alive.
    auto edited = std::make_shared<Chunk>(*entry.blocks);
    edited->Set(local.x, local.y, local.z, id);
    entry.blocks = std::move(edited);
    ++entry.dataVersion;

    // Border edits change neighbor meshes too — face culling and AO both
    // reach one block across the seam. Their data didn't change, but
    // bumping dataVersion is exactly "this chunk needs a fresh mesh".
    const glm::ivec3 off{
        local.x == 0 ? -1 : (local.x == Chunk::kSize - 1 ? 1 : 0),
        local.y == 0 ? -1 : (local.y == Chunk::kSize - 1 ? 1 : 0),
        local.z == 0 ? -1 : (local.z == Chunk::kSize - 1 ? 1 : 0),
    };
    for (int mask = 1; mask < 8; ++mask) {
        const glm::ivec3 d{(mask & 1) ? off.x : 0, (mask & 2) ? off.y : 0,
                           (mask & 4) ? off.z : 0};
        if (d == glm::ivec3{0}) {
            continue; // axis not on a border (duplicate bumps are harmless anyway)
        }
        const auto neighborIt = m_chunks.find(cc + d);
        if (neighborIt != m_chunks.end() && neighborIt->second.blocks) {
            ++neighborIt->second.dataVersion;
        }
    }
}

std::optional<World::RaycastHit> World::RaycastBlocks(const glm::vec3& origin,
                                                      const glm::vec3& dir,
                                                      float maxDistance) const {
    const glm::vec3 d = glm::normalize(dir);
    glm::ivec3 cell{static_cast<int>(std::floor(origin.x)),
                    static_cast<int>(std::floor(origin.y)),
                    static_cast<int>(std::floor(origin.z))};

    glm::ivec3 step{0};
    glm::vec3 tMax{std::numeric_limits<float>::infinity()};
    glm::vec3 tDelta{std::numeric_limits<float>::infinity()};
    for (int i = 0; i < 3; ++i) {
        if (d[i] > 0.0f) {
            step[i] = 1;
            tDelta[i] = 1.0f / d[i];
            tMax[i] = (static_cast<float>(cell[i] + 1) - origin[i]) / d[i];
        } else if (d[i] < 0.0f) {
            step[i] = -1;
            tDelta[i] = -1.0f / d[i];
            tMax[i] = (static_cast<float>(cell[i]) - origin[i]) / d[i];
        }
    }

    while (true) {
        const int axis = (tMax.x < tMax.y) ? (tMax.x < tMax.z ? 0 : 2)
                                           : (tMax.y < tMax.z ? 1 : 2);
        if (tMax[axis] > maxDistance) {
            return std::nullopt;
        }
        cell[axis] += step[axis];
        tMax[axis] += tDelta[axis];
        if (IsSolid(cell.x, cell.y, cell.z)) {
            glm::ivec3 normal{0};
            normal[axis] = -step[axis];
            return RaycastHit{cell, normal};
        }
    }
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
    m_chunks.emplace(coord, ChunkEntry{}); // blocks stays null until the job lands
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
    entry.meshingVersion = entry.dataVersion;
    ++m_jobsInFlight;
    m_pool.Submit([this, coord, version = entry.dataVersion, snapshot = SnapshotFor(coord)] {
        MeshResult result{coord, version, ChunkMesher::Build(snapshot)};
        std::lock_guard lock(m_completedMutex);
        m_completedMesh.push_back(std::move(result));
    });
}

void World::UploadMesh(ChunkEntry& entry, const ChunkMesh& mesh) {
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

    for (auto& gen : gens) {
        const auto it = m_chunks.find(gen.coord);
        if (it == m_chunks.end() || it->second.blocks) {
            continue; // unloaded mid-job, or a duplicate after unload+reload
        }
        it->second.blocks = std::move(gen.blocks);
        it->second.dataVersion = 1;
    }
    for (auto& meshed : meshes) {
        const auto it = m_chunks.find(meshed.coord);
        if (it == m_chunks.end()) {
            continue; // unloaded mid-job
        }
        ChunkEntry& entry = it->second;
        if (meshed.version == entry.dataVersion) {
            UploadMesh(entry, meshed.mesh);
            entry.meshedVersion = meshed.version;
        }
        // Stale results (version mismatch after an edit) are dropped; the
        // old mesh keeps rendering until the rebuilt one lands. Either way
        // the job is resolved, so clear meshingVersion to allow resubmit.
        if (entry.meshingVersion == meshed.version) {
            entry.meshingVersion = 0;
        }
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
                if (!inViewRadius) {
                    continue;
                }
                const ChunkEntry& entry = it->second;
                const bool needsMesh = entry.blocks && entry.meshedVersion != entry.dataVersion;
                if (!entry.blocks || needsMesh) {
                    ++m_pendingMeshes;
                }
                if (needsMesh && entry.meshingVersion != entry.dataVersion &&
                    NeighborsHaveData(coord)) {
                    toMesh.emplace_back(dist2, coord);
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
