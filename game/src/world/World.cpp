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

// Chunk meshes are quads sharing one index pattern (the mesher rotates
// vertex order for the AO diagonal flip, see EmitQuad), so the pool needs
// no per-chunk index data. The initial capacity comfortably fits a radius
// 12 view (~0.8M vertices, 8 B each packed); the pool grows if exceeded.
constexpr uint32_t kQuadPattern[] = {0, 1, 2, 2, 3, 0};
constexpr uint32_t kInitialPoolVertices = 1u << 20;

// Two packed uint32s, decoded bitwise in chunk.vert (see ChunkVertex).
vox::BufferLayout ChunkVertexLayout() {
    return {
        {vox::ShaderDataType::UInt, "a_data0"},
        {vox::ShaderDataType::UInt, "a_data1"},
    };
}

int FloorDivChunk(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / static_cast<float>(Chunk::kSize)));
}

// Chebyshev distances from the center column to a LOD column, which covers
// regular columns [2c, 2c+1] on each axis: "near" to its closest regular
// column (outer ring bounds), "far" to its farthest (inner bounds — any
// part outside the detail radius means the column must be drawn).
int LodAxisDist(int lodC, int center, bool far) {
    const int lo = 2 * lodC;
    const int hi = lo + 1;
    const int dLo = std::abs(lo - center);
    const int dHi = std::abs(hi - center);
    if (far) {
        return std::max(dLo, dHi);
    }
    return center >= lo && center <= hi ? 0 : std::min(dLo, dHi);
}

int LodNearDist(const glm::ivec2& lodColumn, int centerX, int centerZ) {
    return std::max(LodAxisDist(lodColumn.x, centerX, false),
                    LodAxisDist(lodColumn.y, centerZ, false));
}

int LodFarDist(const glm::ivec2& lodColumn, int centerX, int centerZ) {
    return std::max(LodAxisDist(lodColumn.x, centerX, true),
                    LodAxisDist(lodColumn.y, centerZ, true));
}

// LOD chunks are meshed as if fully sky-lit: at LOD distances everything
// visible is outdoor surface, and the real lighting pipeline only covers
// loaded full-detail columns.
const std::shared_ptr<const ChunkLight>& FullBrightLight() {
    static const std::shared_ptr<const ChunkLight> light = [] {
        auto l = std::make_shared<ChunkLight>();
        for (int y = 0; y < Chunk::kSize; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                for (int x = 0; x < Chunk::kSize; ++x) {
                    l->Set(x, y, z, ChunkLight::Pack(15, 0));
                }
            }
        }
        return l;
    }();
    return light;
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

World::World(int defaultSeed, std::filesystem::path saveDir)
    : m_save(std::move(saveDir), defaultSeed), m_generator(m_save.Seed()),
      m_meshPool(ChunkVertexLayout(), kQuadPattern, 4, kInitialPoolVertices),
      m_lastAutosave(std::chrono::steady_clock::now()) {
    // Furnace block entities saved with the world; slots holding ids from
    // a newer build than this one are dropped rather than crashing.
    for (const auto& r : m_save.GetFurnaces()) {
        FurnaceState state;
        ItemStack* slots[3] = {&state.input, &state.fuel, &state.output};
        for (int slot = 0; slot < 3; ++slot) {
            if (ItemExists(r.id[slot]) && r.count[slot] > 0) {
                *slots[slot] = {r.id[slot], r.count[slot], std::max(r.damage[slot], 0)};
            }
        }
        state.burnTicks = std::max(r.burnTicks, 0);
        state.burnTotal = std::max(r.burnTotal, 0);
        state.cookTicks = std::clamp(r.cookTicks, 0, furnace::kCookTicks);
        m_furnaces.emplace(r.pos, state);
    }
}

World::~World() {
    // Blocks still in flight settle instantly so they persist.
    for (const auto& falling : m_fallingBlocks) {
        int restY = std::clamp(static_cast<int>(std::lround(falling.y)), 0, kHeightBlocks - 1);
        while (restY < kHeightBlocks && IsSolid(falling.x, restY, falling.z)) {
            ++restY;
        }
        if (restY < kHeightBlocks) {
            SetBlock({falling.x, restY, falling.z}, falling.id);
        }
    }
    // Workers may still be running (the pool joins after this body), but
    // they never touch the chunk map or the save store.
    SaveEditedChunks();
    m_save.Flush(true);
}

void World::SaveEditedChunks() {
    for (auto& [coord, entry] : m_chunks) {
        if (entry.edited && entry.blocks) {
            m_save.Put(coord, *entry.blocks);
            entry.edited = false;
        }
    }
    SaveFurnaces();
}

void World::SaveFurnaces() {
    std::vector<WorldSave::FurnaceRecord> records;
    records.reserve(m_furnaces.size());
    for (const auto& [pos, state] : m_furnaces) {
        WorldSave::FurnaceRecord r;
        r.pos = pos;
        const ItemStack* slots[3] = {&state.input, &state.fuel, &state.output};
        for (int slot = 0; slot < 3; ++slot) {
            r.id[slot] = slots[slot]->id;
            r.count[slot] = slots[slot]->count;
            r.damage[slot] = slots[slot]->damage;
        }
        r.burnTicks = state.burnTicks;
        r.burnTotal = state.burnTotal;
        r.cookTicks = state.cookTicks;
        records.push_back(r);
    }
    m_save.SetFurnaces(std::move(records));
}

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
    const BlockId oldId = entry.blocks->Get(local.x, local.y, local.z);
    if (oldId == id) {
        return;
    }

    // A furnace replaced by anything that isn't a furnace (the lit/unlit
    // swap stays in the family) spills its slots as drops and loses its
    // block-entity state — vanilla's breakBlock behavior.
    const auto isFurnace = [](BlockId b) { return b == blocks::Furnace || b == blocks::LitFurnace; };
    if (isFurnace(oldId) && !isFurnace(id)) {
        if (const auto furnaceIt = m_furnaces.find(worldPos); furnaceIt != m_furnaces.end()) {
            for (const ItemStack* slot : {&furnaceIt->second.input, &furnaceIt->second.fuel,
                                          &furnaceIt->second.output}) {
                SpawnBlockDrop(worldPos, slot->id, slot->count, slot->damage);
            }
            m_furnaces.erase(furnaceIt);
        }
    }

    // Copy-on-write: in-flight snapshots keep the old chunk alive.
    auto edited = std::make_shared<Chunk>(*entry.blocks);
    edited->Set(local.x, local.y, local.z, id);
    entry.blocks = std::move(edited);
    ++entry.dataVersion;
    entry.edited = true;

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

    // Every edit wakes its neighborhood: gravity and water flow react via
    // scheduled ticks, and their own SetBlocks keep the cascade going.
    // Liquids pace themselves slower than gravity blocks (Minecraft-ish).
    constexpr int kEditUpdateDelay = 2;
    constexpr int kLiquidUpdateDelay = 4;
    constexpr glm::ivec3 kFaceDirs[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    };
    const auto wake = [&](const glm::ivec3& pos) {
        const bool liquid = BlockRegistry::Get().Def(GetBlock(pos.x, pos.y, pos.z)).liquid;
        ScheduleBlockUpdate(pos, liquid ? kLiquidUpdateDelay : kEditUpdateDelay);
    };
    wake(worldPos);
    for (const auto& dir : kFaceDirs) {
        wake(worldPos + dir);
    }
}

void World::ScheduleBlockUpdate(const glm::ivec3& worldPos, int delayTicks) {
    m_blockUpdates.push({m_simTick + static_cast<uint64_t>(std::max(1, delayTicks)), worldPos});
}

void World::Tick() {
    ++m_simTick;
    // Cap the work per tick; the queue carries the rest. Updates pushed
    // while processing are due in the future, so this can't spin forever.
    constexpr int kMaxUpdatesPerTick = 512;
    for (int i = 0; i < kMaxUpdatesPerTick && !m_blockUpdates.empty() &&
                    m_blockUpdates.top().due <= m_simTick;
         ++i) {
        const glm::ivec3 pos = m_blockUpdates.top().pos;
        m_blockUpdates.pop();
        ProcessBlockUpdate(pos);
    }

    // Falling blocks: accelerate down, settle when the cell below the one
    // being entered is solid. Spawn order is bottom-first for a broken
    // column, so stacked sand restacks in order.
    constexpr float kTickDt = 0.05f; // 20 TPS
    constexpr float kFallAccel = 18.0f;
    constexpr float kFallTerminal = 40.0f;
    constexpr float kFallTerminalLiquid = 4.0f; // sand sinks gently in water

    const auto& registry = BlockRegistry::Get();
    for (size_t i = 0; i < m_fallingBlocks.size();) {
        FallingBlock& falling = m_fallingBlocks[i];
        if (falling.landed) {
            // Block already placed — linger (still drawn) until the chunk
            // mesh shows it, then disappear without a gap.
            if (MeshCaughtUp(falling.syncCell, falling.syncVersion)) {
                m_fallingBlocks.erase(m_fallingBlocks.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
            continue;
        }
        falling.prevY = falling.y;

        const int cellY = static_cast<int>(std::floor(falling.y));
        const glm::ivec3 cc{falling.x >> 4,
                            std::clamp(cellY, 0, kHeightBlocks - 1) >> 4, falling.z >> 4};
        const auto chunkIt = m_chunks.find(cc);
        if (chunkIt == m_chunks.end() || !chunkIt->second.blocks) {
            ++i; // ground not loaded — hang in the air until it is
            continue;
        }

        const bool inLiquid = registry.Def(GetBlock(falling.x, cellY, falling.z)).liquid;
        falling.velocity = std::min(falling.velocity + kFallAccel * kTickDt,
                                    inLiquid ? kFallTerminalLiquid : kFallTerminal);
        const float newY = falling.y - falling.velocity * kTickDt;

        bool landed = false;
        for (int level = cellY; level >= static_cast<int>(std::floor(newY)) && !landed;
             --level) {
            if (newY > static_cast<float>(level)) {
                continue; // hasn't reached this cell boundary yet
            }
            if (level - 1 < 0 || IsSolid(falling.x, level - 1, falling.z)) {
                // Settle at `level` — or above it if something solid moved
                // in (the landing SetBlock replaces at worst a liquid).
                int restY = level;
                while (restY < kHeightBlocks && IsSolid(falling.x, restY, falling.z)) {
                    ++restY;
                }
                if (restY < kHeightBlocks) {
                    const glm::ivec3 restCell{falling.x, restY, falling.z};
                    CrushDrops(restCell); // a plant under landing sand pops its drop
                    SetBlock(restCell, falling.id);
                    // Freeze the cube on the placed cell and keep drawing
                    // it until the remesh lands.
                    falling.y = static_cast<float>(restY);
                    falling.prevY = falling.y;
                    falling.syncCell = restCell;
                    falling.syncVersion = DataVersionAt(restCell);
                }
                landed = true;
            }
        }
        if (landed) {
            falling.landed = true;
        } else {
            falling.y = newY;
        }
        ++i;
    }

    TickFurnaces();
    TickItemEntities();
}

void World::TickFurnaces() {
    for (auto it = m_furnaces.begin(); it != m_furnaces.end();) {
        const glm::ivec3 pos = it->first;
        const BlockId at = GetBlock(pos.x, pos.y, pos.z);
        if (at != blocks::Furnace && at != blocks::LitFurnace) {
            const glm::ivec3 cc{pos.x >> 4, pos.y >> 4, pos.z >> 4};
            const auto chunkIt = m_chunks.find(cc);
            if (chunkIt != m_chunks.end() && chunkIt->second.blocks) {
                // Loaded chunk, no furnace block: stale state (the spill
                // path normally erases it first) — drop it.
                it = m_furnaces.erase(it);
                continue;
            }
            ++it; // chunk not loaded — idle until it streams back in
            continue;
        }
        const bool burning = furnace::Tick(it->second);
        if (burning != (at == blocks::LitFurnace)) {
            // Lit/unlit swap; same family, so SetBlock keeps the state.
            // Emission 13 relights through the normal edit path.
            SetBlock(pos, burning ? blocks::LitFurnace : blocks::Furnace);
        }
        ++it;
    }
}

namespace {

// Cheap scatter randomness for item drops — gameplay-only, nothing
// worldgen-deterministic flows through this.
float ItemRand01() {
    static uint32_t s = 0x9E3779B9u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s & 0xFFFFFFu) / 16777215.0f;
}

constexpr float kItemHalf = 0.125f; // the mini cube is 0.25 on a side
constexpr int kItemDespawnAge = 6000;   // 5 minutes, vanilla
constexpr int kItemMergeMax = 64;       // == kMaxStackSize (Inventory.h)

} // namespace

void World::SpawnBlockDrop(const glm::ivec3& cell, uint16_t id, int count, int damage) {
    // Vanilla Block.spawnAsEntity: jitter into the middle half of the cell;
    // EntityItem's constructor adds the small scatter velocity (b/tick
    // 0.2 up, +-0.1 sideways -> b/s at 20 TPS).
    const glm::vec3 pos{static_cast<float>(cell.x) + 0.25f + ItemRand01() * 0.5f,
                        static_cast<float>(cell.y) + 0.25f + ItemRand01() * 0.5f,
                        static_cast<float>(cell.z) + 0.25f + ItemRand01() * 0.5f};
    const glm::vec3 vel{(ItemRand01() - 0.5f) * 4.0f, 4.0f, (ItemRand01() - 0.5f) * 4.0f};
    SpawnItem(pos, vel, id, count, 10, damage);
}

void World::SpawnItem(const glm::vec3& pos, const glm::vec3& vel, uint16_t id, int count,
                      int pickupDelay, int damage) {
    if (id == blocks::Air || count <= 0) {
        return;
    }
    ItemEntity item;
    item.id = id;
    item.count = count;
    item.damage = damage;
    item.pos = pos;
    item.prevPos = pos;
    item.vel = vel;
    item.pickupDelay = pickupDelay;
    item.phase = ItemRand01() * 6.2832f; // vanilla hoverStart: random 0..2pi
    m_itemEntities.push_back(item);
}

void World::TickItemEntities() {
    constexpr float kTickDt = 0.05f;
    constexpr float kGravity = 16.0f; // vanilla 0.04 b/tick^2

    // Axis-separated AABB move against solid blocks (the player's scheme,
    // sized for the item cube). Returns true on collision (velocity zeroed).
    const auto moveAxis = [&](ItemEntity& item, int axis, float delta) {
        if (delta == 0.0f) {
            return false;
        }
        item.pos[axis] += delta;
        const glm::vec3 boxMin = item.pos - glm::vec3{kItemHalf, 0.0f, kItemHalf};
        const glm::vec3 boxMax = item.pos + glm::vec3{kItemHalf, 2.0f * kItemHalf, kItemHalf};
        const glm::ivec3 lo{static_cast<int>(std::floor(boxMin.x + 1e-5f)),
                            static_cast<int>(std::floor(boxMin.y + 1e-5f)),
                            static_cast<int>(std::floor(boxMin.z + 1e-5f))};
        const glm::ivec3 hi{static_cast<int>(std::floor(boxMax.x - 1e-5f)),
                            static_cast<int>(std::floor(boxMax.y - 1e-5f)),
                            static_cast<int>(std::floor(boxMax.z - 1e-5f))};
        bool collided = false;
        float resolved = item.pos[axis];
        for (int by = lo.y; by <= hi.y; ++by) {
            for (int bz = lo.z; bz <= hi.z; ++bz) {
                for (int bx = lo.x; bx <= hi.x; ++bx) {
                    if (!IsSolid(bx, by, bz)) {
                        continue;
                    }
                    collided = true;
                    const int cell = (axis == 0) ? bx : (axis == 1) ? by : bz;
                    if (delta > 0.0f) {
                        const float extent = (axis == 1) ? 2.0f * kItemHalf : kItemHalf;
                        resolved = std::min(resolved, static_cast<float>(cell) - extent - 0.001f);
                    } else {
                        const float extent = (axis == 1) ? 0.0f : kItemHalf;
                        resolved = std::max(resolved, static_cast<float>(cell + 1) + extent + 0.001f);
                    }
                }
            }
        }
        if (collided) {
            item.pos[axis] = resolved;
            item.vel[axis] = 0.0f;
        }
        return collided;
    };

    for (size_t i = 0; i < m_itemEntities.size();) {
        ItemEntity& item = m_itemEntities[i];
        item.prevPos = item.pos;
        ++item.age;
        if (item.pickupDelay > 0) {
            --item.pickupDelay;
        }
        if (item.age >= kItemDespawnAge) {
            m_itemEntities.erase(m_itemEntities.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }

        const glm::ivec3 cell{static_cast<int>(std::floor(item.pos.x)),
                              static_cast<int>(std::floor(item.pos.y + kItemHalf)),
                              static_cast<int>(std::floor(item.pos.z))};
        const glm::ivec3 cc{cell.x >> 4, std::clamp(cell.y, 0, kHeightBlocks - 1) >> 4,
                            cell.z >> 4};
        const auto chunkIt = m_chunks.find(cc);
        if (chunkIt == m_chunks.end() || !chunkIt->second.blocks) {
            ++i; // ground not loaded — hang like falling blocks do
            continue;
        }

        item.vel.y -= kGravity * kTickDt;
        bool onGround = false;
        if (cell.y >= 0 && cell.y < kHeightBlocks && IsSolid(cell.x, cell.y, cell.z)) {
            // Embedded (a block was placed over it, or it spawned inside
            // one): skip collision and float up until free — vanilla's
            // pushOutOfBlocks, simplified to "up".
            item.vel = {0.0f, 2.0f, 0.0f};
            item.pos.y += item.vel.y * kTickDt;
        } else {
            onGround = moveAxis(item, 1, item.vel.y * kTickDt) && item.prevPos.y >= item.pos.y;
            moveAxis(item, 0, item.vel.x * kTickDt);
            moveAxis(item, 2, item.vel.z * kTickDt);
        }

        // Vanilla drag: x0.98/tick, ground friction x0.6 horizontally.
        const float friction = onGround ? 0.6f * 0.98f : 0.98f;
        item.vel.x *= friction;
        item.vel.z *= friction;
        item.vel.y *= 0.98f;

        // Merge with same-id stacks nearby (vanilla: on cell crossing or
        // every 25 ticks; absorbed item keeps the younger age).
        const bool crossedCell = glm::ivec3{glm::floor(item.prevPos)} !=
                                 glm::ivec3{glm::floor(item.pos)};
        if (crossedCell || item.age % 25 == 0) {
            for (size_t j = m_itemEntities.size(); j-- > i + 1;) {
                ItemEntity& other = m_itemEntities[j];
                if (other.id != item.id || other.damage != item.damage ||
                    item.count + other.count > kItemMergeMax ||
                    std::abs(other.pos.x - item.pos.x) > 0.5f ||
                    std::abs(other.pos.y - item.pos.y) > 0.5f ||
                    std::abs(other.pos.z - item.pos.z) > 0.5f) {
                    continue;
                }
                item.count += other.count;
                item.age = std::min(item.age, other.age);
                item.pickupDelay = std::max(item.pickupDelay, other.pickupDelay);
                m_itemEntities.erase(m_itemEntities.begin() + static_cast<ptrdiff_t>(j));
            }
        }
        ++i;
    }
}

void World::CrushDrops(const glm::ivec3& pos) {
    const auto& registry = BlockRegistry::Get();
    const BlockId id = GetBlock(pos.x, pos.y, pos.z);
    const BlockDef& def = registry.Def(id);
    if (def.cross || def.torch) {
        SpawnBlockDrop(pos, def.ResolveDrop(id), 1);
    }
}

uint32_t World::DataVersionAt(const glm::ivec3& worldPos) const {
    const auto it = m_chunks.find({worldPos.x >> 4, worldPos.y >> 4, worldPos.z >> 4});
    return it != m_chunks.end() ? it->second.dataVersion : 0;
}

bool World::MeshCaughtUp(const glm::ivec3& worldPos, uint32_t version) const {
    const auto it = m_chunks.find({worldPos.x >> 4, worldPos.y >> 4, worldPos.z >> 4});
    return it == m_chunks.end() || it->second.meshedVersion >= version;
}

bool World::FallingBlockVisible(const FallingBlock& falling) const {
    // Landed cubes cover the not-yet-remeshed block; in-flight cubes stay
    // hidden while the stale mesh still shows the block they came from.
    return falling.landed || MeshCaughtUp(falling.syncCell, falling.syncVersion);
}

void World::ProcessBlockUpdate(const glm::ivec3& worldPos) {
    if (worldPos.y <= 0 || worldPos.y >= kHeightBlocks) {
        return; // the world floor supports everything
    }
    const glm::ivec3 cc{worldPos.x >> 4, worldPos.y >> 4, worldPos.z >> 4};
    const auto it = m_chunks.find(cc);
    if (it == m_chunks.end() || !it->second.blocks) {
        return; // unloaded mid-flight; edits only happen near the player
    }

    const auto& registry = BlockRegistry::Get();
    const BlockId id = GetBlock(worldPos.x, worldPos.y, worldPos.z);
    const BlockDef& def = registry.Def(id);
    if (def.cross) {
        // Plants pop without proper soil: earth for grass/flowers, sand
        // for the dead bush. Digging out the ground wakes this cell.
        const BlockId below = GetBlock(worldPos.x, worldPos.y - 1, worldPos.z);
        const bool supported = id == blocks::DeadBush
                                   ? below == blocks::Sand
                                   : below == blocks::Grass || below == blocks::Dirt ||
                                         below == blocks::SnowyGrass;
        if (!supported) {
            SpawnBlockDrop(worldPos, def.ResolveDrop(id), 1); // popped plants drop
            SetBlock(worldPos, blocks::Air);
        }
    } else if (id == blocks::Cactus) {
        // Cactus stands on sand or more cactus; breaking a segment pops
        // everything above it, one update at a time.
        const BlockId below = GetBlock(worldPos.x, worldPos.y - 1, worldPos.z);
        if (below != blocks::Sand && below != blocks::Cactus) {
            SpawnBlockDrop(worldPos, def.ResolveDrop(id), 1);
            SetBlock(worldPos, blocks::Air);
        }
    } else if (def.torch) {
        // Torches need solid ground (floor-standing only until block
        // orientation data exists for wall mounts).
        if (!IsSolid(worldPos.x, worldPos.y - 1, worldPos.z)) {
            SpawnBlockDrop(worldPos, def.ResolveDrop(id), 1);
            SetBlock(worldPos, blocks::Air);
        }
    } else if (def.gravity) {
        const BlockId below = GetBlock(worldPos.x, worldPos.y - 1, worldPos.z);
        const BlockDef& belowDef = registry.Def(below);
        if (below == blocks::Air || belowDef.liquid || belowDef.replaceable) {
            // Detach into a falling entity; it becomes a block again on
            // landing. The SetBlock wakes the neighborhood, so sand above
            // follows next tick. The entity stays hidden until the remesh
            // drops the removed block (see FallingBlockVisible).
            SetBlock(worldPos, blocks::Air);
            m_fallingBlocks.push_back({worldPos.x, worldPos.z, static_cast<float>(worldPos.y),
                                       static_cast<float>(worldPos.y), 0.0f, id, worldPos,
                                       DataVersionAt(worldPos), false});
        }
    } else if (def.liquid) {
        UpdateLiquid(worldPos, def.liquidLevel);
    }
}

namespace {

constexpr glm::ivec3 kLiquidSides[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
constexpr int kSlopeFindDistance = 4; // matches BlockDynamicLiquid

} // namespace

void World::UpdateLiquid(const glm::ivec3& worldPos, int level) {
    const auto& registry = BlockRegistry::Get();
    const auto defAt = [&](const glm::ivec3& pos) -> const BlockDef& {
        return registry.Def(GetBlock(pos.x, pos.y, pos.z));
    };
    constexpr glm::ivec3 kUp{0, 1, 0};
    constexpr glm::ivec3 kDown{0, -1, 0};
    const bool fedFromAbove = defAt(worldPos + kUp).liquid;

    // Flow cells re-derive their level from their support: water above
    // sustains a near-full falling column, otherwise the strongest
    // horizontal neighbor minus one (falling neighbors count as full
    // strength, like Minecraft's depth-8 flag). Receding is the same rule
    // — as the support decays, so does this cell, one step per update.
    if (level < 8) {
        int support = fedFromAbove ? 7 : 0;
        int adjacentSources = 0;
        for (const auto& side : kLiquidSides) {
            const glm::ivec3 pos = worldPos + side;
            const BlockDef& neighbor = defAt(pos);
            if (!neighbor.liquid) {
                continue;
            }
            if (neighbor.liquidLevel == 8) {
                ++adjacentSources;
            }
            const int strength = defAt(pos + kUp).liquid ? 8 : neighbor.liquidLevel;
            support = std::max(support, strength - 1);
        }

        // Two sources feeding a cell over solid ground (or a source) mint
        // a new source — ponds self-heal (Minecraft's infinite water).
        const BlockDef& belowDef = defAt(worldPos + kDown);
        if (adjacentSources >= 2 && (belowDef.solid || belowDef.liquidLevel == 8)) {
            SetBlock(worldPos, blocks::Water);
            return;
        }

        if (support != level) {
            SetBlock(worldPos, support <= 0 ? blocks::Air
                                            : blocks::WaterFlows[static_cast<size_t>(support - 1)]);
            return; // the new state acts on its own next update
        }
    }

    // Falling beats sideways: water over air (or a weaker flow) pours
    // straight down as a near-full column.
    if (worldPos.y > 0) {
        const glm::ivec3 below = worldPos + kDown;
        const BlockId belowId = GetBlock(below.x, below.y, below.z);
        const BlockDef& belowDef = registry.Def(belowId);
        if (belowId == blocks::Air || belowDef.replaceable ||
            (belowDef.liquid && belowDef.liquidLevel < 7)) {
            CrushDrops(below); // crushed plants pop their drops
            SetBlock(below, blocks::WaterFlows[6]);
            return;
        }
        // Flows sheet sideways only when resting on SOLID ground. A flow
        // over water is a falling column or an edge pour — it keeps
        // feeding downward and never sheets at altitude. Sources still
        // sheet over water (that's how a breached ocean floods in at
        // every depth).
        if (level < 8 && !belowDef.solid) {
            return;
        }
    }

    // Sideways spread: sources and waterfall landings push full strength,
    // other flows their level minus one — into air or weaker flows only.
    const int spreadLevel = (level == 8 || fedFromAbove) ? 7 : std::min(level - 1, 7);
    if (spreadLevel < 1) {
        return;
    }

    // Slope-seeking (Minecraft's getPossibleFlowDirections): spread only
    // toward the direction(s) whose nearest drop is closest, within
    // kSlopeFindDistance. On flat ground every direction ties and water
    // sheets uniformly; near an edge it beelines for the drop.
    int cost[4];
    int best = 1000;
    for (int d = 0; d < 4; ++d) {
        cost[d] = 1000 + 1; // unenterable sorts after "no hole found"
        const glm::ivec3 pos = worldPos + kLiquidSides[d];
        const BlockDef& neighbor = defAt(pos);
        if (neighbor.solid || neighbor.liquidLevel == 8) {
            continue;
        }
        cost[d] = defAt(pos + kDown).solid ? SlopeDistance(pos, 1, d ^ 1) : 0;
        best = std::min(best, cost[d]);
    }

    for (int d = 0; d < 4; ++d) {
        if (cost[d] != best) {
            continue;
        }
        const glm::ivec3 pos = worldPos + kLiquidSides[d];
        const BlockId neighbor = GetBlock(pos.x, pos.y, pos.z);
        const BlockDef& neighborDef = registry.Def(neighbor);
        if (neighbor == blocks::Air || neighborDef.replaceable ||
            (neighborDef.liquid && neighborDef.liquidLevel < spreadLevel)) {
            CrushDrops(pos); // crushed plants pop their drops
            SetBlock(pos, blocks::WaterFlows[static_cast<size_t>(spreadLevel - 1)]);
        }
    }
}

int World::SlopeDistance(const glm::ivec3& worldPos, int distance, int fromDir) const {
    const auto& registry = BlockRegistry::Get();
    int best = 1000;
    for (int d = 0; d < 4; ++d) {
        if (d == fromDir) {
            continue; // don't walk straight back
        }
        const glm::ivec3 pos = worldPos + kLiquidSides[d];
        const BlockDef& def = registry.Def(GetBlock(pos.x, pos.y, pos.z));
        if (def.solid || def.liquidLevel == 8) {
            continue;
        }
        if (!registry.Def(GetBlock(pos.x, pos.y - 1, pos.z)).solid) {
            return distance; // a drop the flow can reach
        }
        if (distance < kSlopeFindDistance) {
            best = std::min(best, SlopeDistance(pos, distance + 1, d ^ 1));
        }
    }
    return best;
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
        // Solids, plants, and torches are targetable (break/place);
        // liquids and air are not. Plants/torches never collide — only
        // the raycast sees them.
        const BlockDef& def = BlockRegistry::Get().Def(GetBlock(cell.x, cell.y, cell.z));
        if (def.solid || def.cross || def.torch) {
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
    // Saved chunks load instead of generating. The blob is copied into the
    // job on the main thread, so later Puts can't invalidate it.
    const std::vector<uint8_t>* blob = m_save.FindBlob(coord);
    if (blob != nullptr) {
        m_pool.Submit([this, coord, blob = *blob] {
            auto chunk = std::make_shared<Chunk>();
            if (!WorldSave::Decode(blob, *chunk)) {
                GAME_ERROR("Save: corrupt chunk ({}, {}, {}); regenerated", coord.x, coord.y,
                           coord.z);
                m_generator.Generate(*chunk, coord);
            }
            std::lock_guard lock(m_completedMutex);
            m_completedGen.push_back({coord, std::move(chunk)});
        });
        return;
    }
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

void World::UploadMesh(vox::MeshPool::MeshHandle& handle, uint32_t& indexCount,
                       const std::vector<ChunkVertex>& vertices) {
    if (handle != vox::MeshPool::kInvalidMesh) {
        m_meshPool.Free(handle);
        handle = vox::MeshPool::kInvalidMesh;
        indexCount = 0;
    }
    if (vertices.empty()) {
        return;
    }
    handle = m_meshPool.Allocate(vertices.data(), static_cast<uint32_t>(vertices.size()));
    indexCount = m_meshPool.IndexCount(handle);
}

bool World::LodNeighborsReady(const glm::ivec2& lodColumn) const {
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto it = m_lodColumns.find(lodColumn + glm::ivec2{dx, dz});
            if (it == m_lodColumns.end() || !it->second.cells[0]) {
                return false;
            }
        }
    }
    return true;
}

void World::SubmitLodGenerate(const glm::ivec2& lodColumn) {
    m_lodColumns.emplace(lodColumn, LodColumnEntry{});
    ++m_jobsInFlight;
    m_pool.Submit([this, lodColumn] {
        // Generate the 2x2 real columns under this LOD column, then
        // downsample 2x2x2 blocks -> 1 cell. The cell id is the most
        // common non-air block (ties to the topmost), so any solid source
        // block makes the cell solid: LOD terrain is a superset of the
        // real terrain, which keeps the detail/LOD seam free of holes.
        // Liquids only win cells with no solid at all (open water), so the
        // superset rule survives shorelines.
        auto src = std::make_unique<std::array<Chunk, 4 * size_t{kHeightChunks}>>();
        const auto srcAt = [&](int cx, int cz, int cy) -> Chunk& {
            return (*src)[static_cast<size_t>((cz * 2 + cx) * kHeightChunks + cy)];
        };
        for (int cz = 0; cz < 2; ++cz) {
            for (int cx = 0; cx < 2; ++cx) {
                for (int cy = 0; cy < kHeightChunks; ++cy) {
                    m_generator.Generate(
                        srcAt(cx, cz, cy),
                        {lodColumn.x * 2 + cx, cy, lodColumn.y * 2 + cz});
                }
            }
        }

        LodGenResult result{lodColumn, {}};
        for (int ly = 0; ly < kLodHeightChunks; ++ly) {
            auto lod = std::make_shared<Chunk>();
            for (int y = 0; y < Chunk::kSize; ++y) {
                for (int z = 0; z < Chunk::kSize; ++z) {
                    for (int x = 0; x < Chunk::kSize; ++x) {
                        const auto& registry = BlockRegistry::Get();
                        BlockId ids[8];
                        int count = 0;
                        bool anySolid = false;
                        // Top-down so the tie-break favors surface blocks
                        // (grass over the dirt underneath it).
                        for (int dy = 1; dy >= 0; --dy) {
                            const int by = (ly * Chunk::kSize + y) * 2 + dy;
                            for (int dz = 0; dz < 2; ++dz) {
                                for (int dx = 0; dx < 2; ++dx) {
                                    const BlockId id = srcAt(x >> 3, z >> 3, by >> 4)
                                                           .Get((x * 2 + dx) & 15, by & 15,
                                                                (z * 2 + dz) & 15);
                                    // Plants are sub-cell detail — invisible
                                    // at LOD distance, noisy if kept.
                                    if (id != blocks::Air && !registry.Def(id).cross) {
                                        ids[count++] = id;
                                        anySolid |= registry.Def(id).solid;
                                    }
                                }
                            }
                        }
                        BlockId best = blocks::Air;
                        int bestCount = 0;
                        for (int i = 0; i < count; ++i) {
                            if (anySolid && !registry.Def(ids[i]).solid) {
                                continue;
                            }
                            int n = 0;
                            for (int j = 0; j < count; ++j) {
                                n += ids[j] == ids[i];
                            }
                            if (n > bestCount) {
                                bestCount = n;
                                best = ids[i];
                            }
                        }
                        lod->Set(x, y, z, best);
                    }
                }
            }
            result.cells[ly] = std::move(lod);
        }
        std::lock_guard lock(m_completedMutex);
        m_completedLodGen.push_back(std::move(result));
    });
}

void World::SubmitLodMesh(const glm::ivec2& lodColumn) {
    LodColumnEntry& entry = m_lodColumns.at(lodColumn);
    entry.meshInFlight = true;

    // Snapshots are pointer copies, captured on the main thread like
    // regular mesh jobs. Missing vertical neighbors read as air.
    std::array<ChunkSnapshot, kLodHeightChunks> snapshots;
    for (int ly = 0; ly < kLodHeightChunks; ++ly) {
        ChunkSnapshot& snapshot = snapshots[ly];
        snapshot.skyAbove = ly == kLodHeightChunks - 1;
        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = ly + dy;
            if (ny < 0 || ny >= kLodHeightChunks) {
                continue;
            }
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const auto it = m_lodColumns.find(lodColumn + glm::ivec2{dx, dz});
                    if (it != m_lodColumns.end()) {
                        snapshot.chunks[ChunkSnapshot::Index(dx + 1, dy + 1, dz + 1)] =
                            it->second.cells[ny];
                    }
                }
            }
        }
        snapshot.light.fill(FullBrightLight());
    }

    ++m_jobsInFlight;
    m_pool.Submit([this, lodColumn, snapshots = std::move(snapshots)] {
        LodMeshResult result{lodColumn, {}};
        for (int ly = 0; ly < kLodHeightChunks; ++ly) {
            result.meshes[ly] = ChunkMesher::Build(snapshots[ly]);
        }
        std::lock_guard lock(m_completedMutex);
        m_completedLodMesh.push_back(std::move(result));
    });
}

void World::DrainCompletedJobs() {
    std::vector<GenResult> gens;
    std::vector<LightResult> lights;
    std::vector<MeshResult> meshes;
    std::vector<LodGenResult> lodGens;
    std::vector<LodMeshResult> lodMeshes;
    {
        std::lock_guard lock(m_completedMutex);
        gens.swap(m_completedGen);
        lights.swap(m_completedLight);
        meshes.swap(m_completedMesh);
        lodGens.swap(m_completedLodGen);
        lodMeshes.swap(m_completedLodMesh);
    }
    m_jobsInFlight -=
        gens.size() + lights.size() + meshes.size() + lodGens.size() + lodMeshes.size();

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

    // GPU uploads are budgeted per frame: results past the budget carry
    // over (deferral is safe — acceptance is versioned, so anything that
    // went stale meanwhile just drops when it's finally processed). The
    // first item always goes through so a single huge mesh can't starve.
    constexpr size_t kUploadBudgetBytes = 2u << 20; // per frame
    size_t uploadedBytes = 0;

    const auto processMesh = [&](MeshResult& meshed) {
        const auto it = m_chunks.find(meshed.coord);
        if (it == m_chunks.end()) {
            return; // unloaded mid-job
        }
        ChunkEntry& entry = it->second;
        if (meshed.version == entry.dataVersion) {
            uploadedBytes += (meshed.mesh.vertices.size() +
                              meshed.mesh.transparentVertices.size()) *
                             sizeof(ChunkVertex);
            UploadMesh(entry.mesh, entry.indexCount, meshed.mesh.vertices);
            UploadMesh(entry.meshT, entry.indexCountT, meshed.mesh.transparentVertices);
            entry.meshedVersion = meshed.version;
            entry.visibility = meshed.mesh.visibility;
        }
        // Stale results (version mismatch after an edit or light change)
        // are dropped; the old mesh keeps rendering until the rebuilt one
        // lands. Either way the job is resolved, so allow resubmit.
        if (entry.meshingVersion == meshed.version) {
            entry.meshingVersion = 0;
        }
    };

    std::vector<MeshResult> stillDeferred;
    for (auto* batch : {&m_deferredMesh, &meshes}) {
        for (auto& meshed : *batch) {
            if (uploadedBytes < kUploadBudgetBytes) {
                processMesh(meshed);
            } else {
                stillDeferred.push_back(std::move(meshed));
            }
        }
    }
    m_deferredMesh = std::move(stillDeferred);

    for (auto& lodGen : lodGens) {
        const auto it = m_lodColumns.find(lodGen.column);
        if (it == m_lodColumns.end() || it->second.cells[0]) {
            continue; // unloaded mid-job, or a duplicate after unload+reload
        }
        it->second.cells = std::move(lodGen.cells);
    }

    // LOD meshes share the frame's upload budget (lowest priority, like
    // their jobs). meshInFlight stays set while deferred, which also keeps
    // the rescan from resubmitting.
    const auto processLodMesh = [&](LodMeshResult& lodMeshed) {
        const auto it = m_lodColumns.find(lodMeshed.column);
        if (it == m_lodColumns.end()) {
            return; // unloaded mid-job; meshes were never uploaded
        }
        LodColumnEntry& entry = it->second;
        entry.meshInFlight = false;
        if (!entry.cells[0] || entry.meshed) {
            return; // recreated mid-job (let the rescan remesh) or duplicate
        }
        for (int ly = 0; ly < kLodHeightChunks; ++ly) {
            uploadedBytes += (lodMeshed.meshes[ly].vertices.size() +
                              lodMeshed.meshes[ly].transparentVertices.size()) *
                             sizeof(ChunkVertex);
            UploadMesh(entry.mesh[ly], entry.indexCount[ly], lodMeshed.meshes[ly].vertices);
            UploadMesh(entry.meshT[ly], entry.indexCountT[ly],
                       lodMeshed.meshes[ly].transparentVertices);
        }
        entry.meshed = true;
    };

    std::vector<LodMeshResult> lodStillDeferred;
    for (auto* batch : {&m_deferredLodMesh, &lodMeshes}) {
        for (auto& lodMeshed : *batch) {
            if (uploadedBytes < kUploadBudgetBytes) {
                processLodMesh(lodMeshed);
            } else {
                lodStillDeferred.push_back(std::move(lodMeshed));
            }
        }
    }
    m_deferredLodMesh = std::move(lodStillDeferred);
}

namespace {

// Back-to-front by item center for the blended pass. Chunk-level ordering
// is enough for axis-aligned water sheets; faces inside one chunk aren't
// sorted.
void SortBackToFront(const glm::vec3& eye, std::vector<vox::MeshPool::DrawItem>& items) {
    std::sort(items.begin(), items.end(),
              [&](const vox::MeshPool::DrawItem& a, const vox::MeshPool::DrawItem& b) {
                  const float halfA = a.perDraw.w * Chunk::kSize * 0.5f;
                  const float halfB = b.perDraw.w * Chunk::kSize * 0.5f;
                  const glm::vec3 ca = glm::vec3(a.perDraw) + halfA;
                  const glm::vec3 cb = glm::vec3(b.perDraw) + halfB;
                  return glm::dot(ca - eye, ca - eye) > glm::dot(cb - eye, cb - eye);
              });
}

} // namespace

void World::CollectVisibleChunks(const glm::vec3& eye, const vox::Frustum& frustum,
                                 bool occlusion, std::vector<vox::MeshPool::DrawItem>& out,
                                 std::vector<vox::MeshPool::DrawItem>& outTransparent) {
    out.clear();
    outTransparent.clear();
    const int centerX = FloorDivChunk(eye.x);
    const int centerZ = FloorDivChunk(eye.z);

    if (!occlusion) {
        for (const auto& [coord, entry] : m_chunks) {
            if (entry.mesh == vox::MeshPool::kInvalidMesh &&
                entry.meshT == vox::MeshPool::kInvalidMesh) {
                continue;
            }
            const glm::vec3 min = glm::vec3(coord * Chunk::kSize);
            if (frustum.IntersectsAABB(min, min + glm::vec3(Chunk::kSize))) {
                if (entry.mesh != vox::MeshPool::kInvalidMesh) {
                    out.push_back({entry.mesh, glm::vec4(min, 1.0f)});
                }
                if (entry.meshT != vox::MeshPool::kInvalidMesh) {
                    outTransparent.push_back({entry.meshT, glm::vec4(min, 1.0f)});
                }
            }
        }
        AppendLodDraws(centerX, centerZ, frustum, out, outTransparent);
        SortBackToFront(eye, outTransparent);
        return;
    }

    const int eyeY = FloorDivChunk(eye.y);
    constexpr int kSpan = 2 * kViewRadius + 1;
    constexpr uint8_t kSeedFace = 6;

    if (m_visitGrid.size() != size_t{kSpan} * kSpan * kHeightChunks) {
        m_visitGrid.assign(size_t{kSpan} * kSpan * kHeightChunks, 0);
    }
    ++m_visitStamp;
    m_bfsQueue.clear();

    // Enqueues + draws a chunk the walk reached. Traversal is bounded by
    // the view square; the frustum gates only the draw-list insert.
    const auto visit = [&](const glm::ivec3& coord, uint8_t enterFace, uint8_t dirMask) {
        if (coord.y < 0 || coord.y >= kHeightChunks ||
            std::abs(coord.x - centerX) > kViewRadius ||
            std::abs(coord.z - centerZ) > kViewRadius) {
            return;
        }
        uint32_t& stamp =
            m_visitGrid[(static_cast<size_t>(coord.y) * kSpan + (coord.z - centerZ + kViewRadius)) *
                            kSpan +
                        (coord.x - centerX + kViewRadius)];
        if (stamp == m_visitStamp) {
            return;
        }
        stamp = m_visitStamp;
        m_bfsQueue.push_back({coord, enterFace, dirMask});

        const auto it = m_chunks.find(coord);
        if (it != m_chunks.end()) {
            const glm::vec3 min = glm::vec3(coord * Chunk::kSize);
            if ((it->second.mesh != vox::MeshPool::kInvalidMesh ||
                 it->second.meshT != vox::MeshPool::kInvalidMesh) &&
                frustum.IntersectsAABB(min, min + glm::vec3(Chunk::kSize))) {
                if (it->second.mesh != vox::MeshPool::kInvalidMesh) {
                    out.push_back({it->second.mesh, glm::vec4(min, 1.0f)});
                }
                if (it->second.meshT != vox::MeshPool::kInvalidMesh) {
                    outTransparent.push_back({it->second.meshT, glm::vec4(min, 1.0f)});
                }
            }
        }
    };

    if (eyeY >= kHeightChunks) {
        // Above the world everything is open sky: every column's top chunk
        // is a seed, entered through its top face.
        for (int dz = -kViewRadius; dz <= kViewRadius; ++dz) {
            for (int dx = -kViewRadius; dx <= kViewRadius; ++dx) {
                visit({centerX + dx, kHeightChunks - 1, centerZ + dz},
                      static_cast<uint8_t>(BlockFace::PosY), 0);
            }
        }
    } else if (eyeY < 0) {
        for (int dz = -kViewRadius; dz <= kViewRadius; ++dz) {
            for (int dx = -kViewRadius; dx <= kViewRadius; ++dx) {
                visit({centerX + dx, 0, centerZ + dz}, static_cast<uint8_t>(BlockFace::NegY), 0);
            }
        }
    } else {
        visit({centerX, eyeY, centerZ}, kSeedFace, 0);
    }

    // BlockFace order: +X, -X, +Y, -Y, +Z, -Z.
    constexpr glm::ivec3 kFaceDirs[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    };
    for (size_t head = 0; head < m_bfsQueue.size(); ++head) {
        const VisitNode node = m_bfsQueue[head]; // copy: visit() reallocates the queue
        const auto it = m_chunks.find(node.coord);
        // Unknown interiors (not yet meshed) traverse permissively: brief
        // over-draw at stream edges instead of chunks blinking in late.
        const VisibilityBits vis = (it != m_chunks.end() && it->second.meshedVersion != 0)
                                       ? it->second.visibility
                                       : kAllFacesConnected;
        for (int d = 0; d < 6; ++d) {
            if ((node.dirMask >> OppositeFace(d)) & 1) {
                continue; // never step back toward the camera
            }
            if (node.enterFace != kSeedFace && node.enterFace != d &&
                !FacesConnected(vis, node.enterFace, d)) {
                continue;
            }
            visit(node.coord + kFaceDirs[d], static_cast<uint8_t>(OppositeFace(d)),
                  static_cast<uint8_t>(node.dirMask | (1u << d)));
        }
    }

    AppendLodDraws(centerX, centerZ, frustum, out, outTransparent);
    SortBackToFront(eye, outTransparent);
}

bool World::DetailMeshedUnder(const glm::ivec2& lodColumn) const {
    for (int dz = 0; dz < kLodScale; ++dz) {
        for (int dx = 0; dx < kLodScale; ++dx) {
            for (int cy = 0; cy < kHeightChunks; ++cy) {
                const auto it = m_chunks.find(
                    glm::ivec3{lodColumn.x * kLodScale + dx, cy, lodColumn.y * kLodScale + dz});
                if (it == m_chunks.end() || it->second.meshedVersion == 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

void World::AppendLodDraws(int centerX, int centerZ, const vox::Frustum& frustum,
                           std::vector<vox::MeshPool::DrawItem>& out,
                           std::vector<vox::MeshPool::DrawItem>& outTransparent) const {
    constexpr float kLodChunkSize = static_cast<float>(Chunk::kSize * kLodScale);
    for (const auto& [lodColumn, entry] : m_lodColumns) {
        if (!entry.meshed) {
            continue;
        }
        // Columns inside the detail radius keep drawing until the real
        // terrain under them has meshed — no hole ring chasing a fast
        // player. The reverse handover (detail still meshed while LOD
        // appears) can't show holes thanks to the solid-biased downsample.
        if (LodFarDist(lodColumn, centerX, centerZ) <= kViewRadius &&
            DetailMeshedUnder(lodColumn)) {
            continue;
        }
        for (int ly = 0; ly < kLodHeightChunks; ++ly) {
            if (entry.mesh[ly] == vox::MeshPool::kInvalidMesh &&
                entry.meshT[ly] == vox::MeshPool::kInvalidMesh) {
                continue;
            }
            const glm::vec3 min = glm::vec3(lodColumn.x, ly, lodColumn.y) * kLodChunkSize;
            if (frustum.IntersectsAABB(min, min + glm::vec3(kLodChunkSize))) {
                if (entry.mesh[ly] != vox::MeshPool::kInvalidMesh) {
                    out.push_back(
                        {entry.mesh[ly], glm::vec4(min, static_cast<float>(kLodScale))});
                }
                if (entry.meshT[ly] != vox::MeshPool::kInvalidMesh) {
                    outTransparent.push_back(
                        {entry.meshT[ly], glm::vec4(min, static_cast<float>(kLodScale))});
                }
            }
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
        const auto it = m_chunks.find(coord);
        if (it->second.edited && it->second.blocks) {
            m_save.Put(coord, *it->second.blocks);
        }
        if (it->second.mesh != vox::MeshPool::kInvalidMesh) {
            m_meshPool.Free(it->second.mesh);
        }
        if (it->second.meshT != vox::MeshPool::kInvalidMesh) {
            m_meshPool.Free(it->second.meshT);
        }
        m_chunks.erase(it);
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

    std::vector<glm::ivec2> lodToErase;
    for (const auto& [lodColumn, entry] : m_lodColumns) {
        if (LodNearDist(lodColumn, centerX, centerZ) > kLodDataOuter ||
            LodFarDist(lodColumn, centerX, centerZ) <= kLodDataInner) {
            lodToErase.push_back(lodColumn);
        }
    }
    for (const auto& lodColumn : lodToErase) {
        const auto it = m_lodColumns.find(lodColumn);
        for (int ly = 0; ly < kLodHeightChunks; ++ly) {
            if (it->second.mesh[ly] != vox::MeshPool::kInvalidMesh) {
                m_meshPool.Free(it->second.mesh[ly]);
            }
            if (it->second.meshT[ly] != vox::MeshPool::kInvalidMesh) {
                m_meshPool.Free(it->second.meshT[ly]);
            }
        }
        m_lodColumns.erase(it);
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

    // LOD work, lowest priority: meshes need 3x3 data, so the data ring is
    // a LOD column wider than the draw ring on each side (see the erase
    // pass). The scan is in LOD-grid units around the center's LOD column.
    std::vector<std::pair<int, glm::ivec2>> lodToGenerate;
    std::vector<std::pair<int, glm::ivec2>> lodToMesh;
    {
        const glm::ivec2 lodCenter{centerX >> 1, centerZ >> 1};
        constexpr int kLodScan = kLodDataOuter / kLodScale + 1;
        for (int dz = -kLodScan; dz <= kLodScan; ++dz) {
            for (int dx = -kLodScan; dx <= kLodScan; ++dx) {
                const glm::ivec2 lodColumn = lodCenter + glm::ivec2{dx, dz};
                const int nearDist = LodNearDist(lodColumn, centerX, centerZ);
                if (nearDist > kLodDataOuter ||
                    LodFarDist(lodColumn, centerX, centerZ) <= kLodDataInner) {
                    continue;
                }
                const auto it = m_lodColumns.find(lodColumn);
                if (it == m_lodColumns.end()) {
                    lodToGenerate.emplace_back(nearDist, lodColumn);
                    continue;
                }
                const LodColumnEntry& entry = it->second;
                if (nearDist <= kLodRadius && entry.cells[0] && !entry.meshed &&
                    !entry.meshInFlight && LodNeighborsReady(lodColumn)) {
                    lodToMesh.emplace_back(nearDist, lodColumn);
                }
            }
        }
    }

    auto byDistance = [](const auto& a, const auto& b) { return a.first < b.first; };
    std::sort(toGenerate.begin(), toGenerate.end(), byDistance);
    std::sort(toLight.begin(), toLight.end(), byDistance);
    std::sort(toMesh.begin(), toMesh.end(), byDistance);
    std::sort(lodToGenerate.begin(), lodToGenerate.end(), byDistance);
    std::sort(lodToMesh.begin(), lodToMesh.end(), byDistance);

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
    for (const auto& [dist, lodColumn] : lodToMesh) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitLodMesh(lodColumn);
    }
    for (const auto& [dist, lodColumn] : lodToGenerate) {
        if (m_jobsInFlight >= maxInFlight) {
            break;
        }
        SubmitLodGenerate(lodColumn);
    }

    // Persistence: unload already Put()s edited chunks as they leave the
    // ring; the periodic sweep covers chunks that stay loaded (so a crash
    // loses at most kAutosaveInterval of edits). Flush debounces itself.
    constexpr auto kAutosaveInterval = std::chrono::seconds(30);
    const auto now = std::chrono::steady_clock::now();
    if (now - m_lastAutosave >= kAutosaveInterval) {
        m_lastAutosave = now;
        SaveEditedChunks();
    }
    m_save.Flush(false);
}

} // namespace vc
