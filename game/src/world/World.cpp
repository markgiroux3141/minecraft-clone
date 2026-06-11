#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "vox/core/Log.h"

namespace vc {

namespace {

// Cap on generation + lighting + meshing jobs in flight, as a multiple of
// the worker count. Keeps the queue short so streaming stays responsive to
// camera movement (work is re-prioritized nearest-first every Update),
// while leaving enough queued that workers never starve between frames.
constexpr size_t kInFlightPerWorker = 4;

int FloorDivChunk(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / static_cast<float>(Chunk::kSize)));
}

// Does the border slab of light values facing (axis, side) differ between
// the two sections? Used to bump only the neighbor meshes that can
// actually see a light change — neighbors sample at most one cell across
// the seam.
bool FaceSlabDiffers(const ChunkLight& a, const ChunkLight& b, int axis, int side) {
    const int fixed = side > 0 ? Chunk::kSize - 1 : 0;
    for (int i = 0; i < Chunk::kSize; ++i) {
        for (int j = 0; j < Chunk::kSize; ++j) {
            int c[3];
            c[axis] = fixed;
            c[(axis + 1) % 3] = i;
            c[(axis + 2) % 3] = j;
            if (a.Get(c[0], c[1], c[2]) != b.Get(c[0], c[1], c[2])) {
                return true;
            }
        }
    }
    return false;
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

    // Immediate single-cell light estimate, so the remesh this edit just
    // triggered doesn't render newly exposed faces pitch black while the
    // proper flood fill recomputes a few frames later.
    if (entry.light) {
        const BlockDef& def = BlockRegistry::Get().Def(id);
        int sky = 0;
        int block = def.emission;
        if (!def.opaque) {
            constexpr glm::ivec3 kDirs[6] = {
                {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
            };
            for (const auto& dir : kDirs) {
                const uint8_t neighbor = PackedLightAt(worldPos + dir);
                sky = std::max(sky, ChunkLight::Sky(neighbor) - 1);
                block = std::max(block, ChunkLight::Block(neighbor) - 1);
            }
            // Straight-down skylight doesn't attenuate.
            if (ChunkLight::Sky(PackedLightAt(worldPos + glm::ivec3{0, 1, 0})) == 15) {
                sky = 15;
            }
        }
        auto patched = std::make_shared<ChunkLight>(*entry.light);
        patched->Set(local.x, local.y, local.z,
                     ChunkLight::Pack(std::max(sky, 0), std::max(block, 0)));
        entry.light = std::move(patched);
    }

    // Border edits change neighbor meshes too — face culling and AO both
    // reach one block across the seam.
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

    // Light travels at most 15 blocks (path distance), so only dirty the
    // neighbor columns the edit can actually reach. A center-of-chunk edit
    // skips all four diagonals; recomputes that change nothing are caught
    // at landing time, but not running them at all is cheaper still.
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int reachX = dx == 0 ? 0 : (dx > 0 ? Chunk::kSize - local.x : local.x + 1);
            const int reachZ = dz == 0 ? 0 : (dz > 0 ? Chunk::kSize - local.z : local.z + 1);
            if (reachX + reachZ > 15) {
                continue;
            }
            const auto colIt = m_columns.find(glm::ivec2{cc.x + dx, cc.z + dz});
            if (colIt != m_columns.end()) {
                ++colIt->second.dirtySeq;
            }
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

uint8_t World::PackedLightAt(const glm::ivec3& worldPos) const {
    if (worldPos.y >= kHeightBlocks) {
        return ChunkLight::Pack(15, 0);
    }
    if (worldPos.y < 0) {
        return 0;
    }
    const auto it = m_chunks.find(glm::ivec3{worldPos.x >> 4, worldPos.y >> 4, worldPos.z >> 4});
    if (it == m_chunks.end() || !it->second.light) {
        return 0;
    }
    return it->second.light->Get(worldPos.x & 15, worldPos.y & 15, worldPos.z & 15);
}

const Chunk* World::GetChunk(const glm::ivec3& chunkCoord) const {
    const auto it = m_chunks.find(chunkCoord);
    return it != m_chunks.end() ? it->second.blocks.get() : nullptr;
}

bool World::NeighborsReady(const glm::ivec3& coord) const {
    for (int dy = -1; dy <= 1; ++dy) {
        const int ny = coord.y + dy;
        if (ny < 0 || ny >= kHeightChunks) {
            continue; // outside the world counts as air / sky
        }
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto it = m_chunks.find(coord + glm::ivec3{dx, dy, dz});
                if (it == m_chunks.end() || !it->second.blocks || !it->second.light) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool World::ColumnHasData(const glm::ivec2& column) const {
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int cy = 0; cy < kHeightChunks; ++cy) {
                const auto it = m_chunks.find(glm::ivec3{column.x + dx, cy, column.y + dz});
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
    snapshot.skyAbove = coord.y == kHeightChunks - 1;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto it = m_chunks.find(coord + glm::ivec3{dx, dy, dz});
                if (it != m_chunks.end()) {
                    const int slot = ChunkSnapshot::Index(dx + 1, dy + 1, dz + 1);
                    snapshot.chunks[slot] = it->second.blocks;
                    snapshot.light[slot] = it->second.light;
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

void World::SubmitLight(const glm::ivec2& column) {
    ColumnEntry& entry = m_columns.at(column);
    entry.lightingSeq = entry.dirtySeq;

    ColumnLightInput input;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int cy = 0; cy < kHeightChunks; ++cy) {
                const auto it = m_chunks.find(glm::ivec3{column.x + dx, cy, column.y + dz});
                if (it != m_chunks.end()) {
                    input.chunks[(dz + 1) * 3 + (dx + 1)][cy] = it->second.blocks;
                }
            }
        }
    }

    ++m_jobsInFlight;
    m_pool.Submit([this, column, version = entry.dirtySeq, input = std::move(input)] {
        LightResult result{column, version, LightEngine::ComputeColumn(input)};
        std::lock_guard lock(m_completedMutex);
        m_completedLight.push_back(std::move(result));
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
        {vox::ShaderDataType::Float, "a_sky"},
        {vox::ShaderDataType::Float, "a_block"},
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
    std::vector<LightResult> lights;
    std::vector<MeshResult> meshes;
    {
        std::lock_guard lock(m_completedMutex);
        gens.swap(m_completedGen);
        lights.swap(m_completedLight);
        meshes.swap(m_completedMesh);
    }
    m_jobsInFlight -= gens.size() + lights.size() + meshes.size();

    for (auto& gen : gens) {
        const auto it = m_chunks.find(gen.coord);
        if (it == m_chunks.end() || it->second.blocks) {
            continue; // unloaded mid-job, or a duplicate after unload+reload
        }
        it->second.blocks = std::move(gen.blocks);
        it->second.dataVersion = 1;
    }

    for (auto& lit : lights) {
        const auto colIt = m_columns.find(lit.column);
        if (colIt == m_columns.end()) {
            continue; // column unloaded mid-job
        }
        ColumnEntry& column = colIt->second;
        if (lit.version == column.dirtySeq) {
            for (int cy = 0; cy < kHeightChunks; ++cy) {
                const auto it = m_chunks.find(glm::ivec3{lit.column.x, cy, lit.column.y});
                if (it == m_chunks.end()) {
                    continue;
                }
                ChunkEntry& entry = it->second;
                const ChunkLight* oldLight = entry.light.get();
                const ChunkLight* newLight = lit.light[cy].get();
                if (oldLight && oldLight->Raw() == newLight->Raw()) {
                    continue; // light unchanged — no remesh cascade
                }
                // Neighbor meshes sample at most one cell across the seam,
                // so only the ones behind a changed border slab are stale.
                // Index pairs match BlockFace: axis*2 = +side, axis*2+1 = -side.
                bool faceChanged[6];
                for (int axis = 0; axis < 3; ++axis) {
                    faceChanged[axis * 2] =
                        !oldLight || FaceSlabDiffers(*oldLight, *newLight, axis, +1);
                    faceChanged[axis * 2 + 1] =
                        !oldLight || FaceSlabDiffers(*oldLight, *newLight, axis, -1);
                }
                entry.light = lit.light[cy];
                ++entry.dataVersion; // own mesh always resamples

                const auto seesChange = [&](const glm::ivec3& d) {
                    // A diagonal neighbor only reads the shared edge/corner
                    // cells, which sit in every involved face slab.
                    if (d.x != 0 && !faceChanged[d.x > 0 ? 0 : 1]) return false;
                    if (d.y != 0 && !faceChanged[d.y > 0 ? 2 : 3]) return false;
                    if (d.z != 0 && !faceChanged[d.z > 0 ? 4 : 5]) return false;
                    return true;
                };
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const glm::ivec3 d{dx, dy, dz};
                            if (d == glm::ivec3{0} || !seesChange(d)) {
                                continue;
                            }
                            const auto nIt = m_chunks.find(
                                glm::ivec3{lit.column.x + dx, cy + dy, lit.column.y + dz});
                            if (nIt != m_chunks.end()) {
                                ++nIt->second.dataVersion;
                            }
                        }
                    }
                }
            }
            column.litSeq = lit.version;
        }
        // Stale or accepted, the job is resolved — allow resubmission.
        if (column.lightingSeq == lit.version) {
            column.lightingSeq = 0;
        }
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
        // Stale results (version mismatch after an edit or light change)
        // are dropped; the old mesh keeps rendering until the rebuilt one
        // lands. Either way the job is resolved, so allow resubmit.
        if (entry.meshingVersion == meshed.version) {
            entry.meshingVersion = 0;
        }
    }
}

void World::Update(const glm::vec3& cameraPos) {
    DrainCompletedJobs();

    const int centerX = FloorDivChunk(cameraPos.x);
    const int centerZ = FloorDivChunk(cameraPos.z);

    // Radius layering: meshes need light for their 3x3 columns, light
    // needs blocks for ITS 3x3 columns — so blocks extend two rings past
    // the mesh radius and light one.
    constexpr int kLightRadius = kViewRadius + 1;
    constexpr int kDataRadius = kViewRadius + 2;

    // Unload everything outside its ring. In-flight jobs for erased
    // coords resolve harmlessly: their results are dropped in
    // DrainCompletedJobs, and snapshots keep the data alive until the job
    // finishes.
    std::vector<glm::ivec3> chunksToErase;
    for (const auto& [coord, entry] : m_chunks) {
        const int dist = std::max(std::abs(coord.x - centerX), std::abs(coord.z - centerZ));
        if (dist > kDataRadius) {
            chunksToErase.push_back(coord);
        }
    }
    for (const auto& coord : chunksToErase) {
        m_chunks.erase(coord);
    }
    std::vector<glm::ivec2> columnsToErase;
    for (const auto& [column, entry] : m_columns) {
        const int dist = std::max(std::abs(column.x - centerX), std::abs(column.y - centerZ));
        if (dist > kLightRadius) {
            columnsToErase.push_back(column);
        }
    }
    for (const auto& column : columnsToErase) {
        m_columns.erase(column);
    }

    // Collect missing work, nearest column first.
    std::vector<std::pair<int, glm::ivec3>> toGenerate;
    std::vector<std::pair<int, glm::ivec2>> toLight;
    std::vector<std::pair<int, glm::ivec3>> toMesh;
    m_pendingMeshes = 0;
    for (int dz = -kDataRadius; dz <= kDataRadius; ++dz) {
        for (int dx = -kDataRadius; dx <= kDataRadius; ++dx) {
            const int dist2 = dx * dx + dz * dz;
            const int dist = std::max(std::abs(dx), std::abs(dz));
            const bool inViewRadius = dist <= kViewRadius;

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
                if (!entry.blocks || !entry.light || needsMesh) {
                    ++m_pendingMeshes;
                }
                if (needsMesh && entry.meshingVersion != entry.dataVersion &&
                    NeighborsReady(coord)) {
                    toMesh.emplace_back(dist2, coord);
                }
            }

            if (dist <= kLightRadius) {
                const glm::ivec2 column{centerX + dx, centerZ + dz};
                ColumnEntry& colEntry = m_columns.try_emplace(column).first->second;
                if (colEntry.litSeq != colEntry.dirtySeq &&
                    colEntry.lightingSeq != colEntry.dirtySeq && ColumnHasData(column)) {
                    toLight.emplace_back(dist2, column);
                }
            }
        }
    }

    auto byDistance = [](const auto& a, const auto& b) { return a.first < b.first; };
    std::sort(toGenerate.begin(), toGenerate.end(), byDistance);
    std::sort(toLight.begin(), toLight.end(), byDistance);
    std::sort(toMesh.begin(), toMesh.end(), byDistance);

    // Meshes first (visible geometry), then light (feeds meshes), then
    // generation (feeds everything).
    const size_t maxInFlight = m_pool.ThreadCount() * kInFlightPerWorker;
    for (const auto& [dist2, coord] : toMesh) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitMesh(coord);
    }
    for (const auto& [dist2, column] : toLight) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitLight(column);
    }
    for (const auto& [dist2, coord] : toGenerate) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitGenerate(coord);
    }
}

} // namespace vc
