#include "entity/EntityManager.h"

#include <algorithm>
#include <cmath>

#include "world/World.h"

namespace vc {

void EntityManager::Tick() {
    TickFallingBlocks();
    TickItemEntities();
}

void EntityManager::SettleFallingBlocks() {
    // Blocks still in flight settle instantly so they persist.
    for (const auto& falling : m_fallingBlocks) {
        int restY =
            std::clamp(static_cast<int>(std::lround(falling.y)), 0, World::kHeightBlocks - 1);
        while (restY < World::kHeightBlocks && m_world.IsSolid(falling.x, restY, falling.z)) {
            ++restY;
        }
        if (restY < World::kHeightBlocks) {
            m_world.SetBlock({falling.x, restY, falling.z}, falling.id);
        }
    }
}

void EntityManager::SpawnFallingBlock(const glm::ivec3& worldPos, BlockId id,
                                      uint32_t dataVersion) {
    m_fallingBlocks.push_back({worldPos.x, worldPos.z, static_cast<float>(worldPos.y),
                               static_cast<float>(worldPos.y), 0.0f, id, worldPos, dataVersion,
                               false});
}

void EntityManager::TickFallingBlocks() {
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
            if (m_world.MeshCaughtUp(falling.syncCell, falling.syncVersion)) {
                m_fallingBlocks.erase(m_fallingBlocks.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
            continue;
        }
        falling.prevY = falling.y;

        const int cellY = static_cast<int>(std::floor(falling.y));
        if (!m_world.IsChunkLoaded(falling.x, std::clamp(cellY, 0, World::kHeightBlocks - 1),
                                   falling.z)) {
            ++i; // ground not loaded — hang in the air until it is
            continue;
        }

        const bool inLiquid = registry.Def(m_world.GetBlock(falling.x, cellY, falling.z)).liquid;
        falling.velocity = std::min(falling.velocity + kFallAccel * kTickDt,
                                    inLiquid ? kFallTerminalLiquid : kFallTerminal);
        const float newY = falling.y - falling.velocity * kTickDt;

        bool landed = false;
        for (int level = cellY; level >= static_cast<int>(std::floor(newY)) && !landed; --level) {
            if (newY > static_cast<float>(level)) {
                continue; // hasn't reached this cell boundary yet
            }
            if (level - 1 < 0 || m_world.IsSolid(falling.x, level - 1, falling.z)) {
                // Settle at `level` — or above it if something solid moved
                // in (the landing SetBlock replaces at worst a liquid).
                int restY = level;
                while (restY < World::kHeightBlocks &&
                       m_world.IsSolid(falling.x, restY, falling.z)) {
                    ++restY;
                }
                if (restY < World::kHeightBlocks) {
                    const glm::ivec3 restCell{falling.x, restY, falling.z};
                    CrushDrops(restCell); // a plant under landing sand pops its drop
                    m_world.SetBlock(restCell, falling.id);
                    // Freeze the cube on the placed cell and keep drawing
                    // it until the remesh lands.
                    falling.y = static_cast<float>(restY);
                    falling.prevY = falling.y;
                    falling.syncCell = restCell;
                    falling.syncVersion = m_world.DataVersionAt(restCell);
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

void EntityManager::SpawnBlockDrop(const glm::ivec3& cell, uint16_t id, int count, int damage) {
    // Vanilla Block.spawnAsEntity: jitter into the middle half of the cell;
    // EntityItem's constructor adds the small scatter velocity (b/tick
    // 0.2 up, +-0.1 sideways -> b/s at 20 TPS).
    const glm::vec3 pos{static_cast<float>(cell.x) + 0.25f + ItemRand01() * 0.5f,
                        static_cast<float>(cell.y) + 0.25f + ItemRand01() * 0.5f,
                        static_cast<float>(cell.z) + 0.25f + ItemRand01() * 0.5f};
    const glm::vec3 vel{(ItemRand01() - 0.5f) * 4.0f, 4.0f, (ItemRand01() - 0.5f) * 4.0f};
    SpawnItem(pos, vel, id, count, 10, damage);
}

void EntityManager::SpawnItem(const glm::vec3& pos, const glm::vec3& vel, uint16_t id, int count,
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

void EntityManager::TickItemEntities() {
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
                    if (!m_world.IsSolid(bx, by, bz)) {
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
        if (!m_world.IsChunkLoaded(cell.x, std::clamp(cell.y, 0, World::kHeightBlocks - 1),
                                   cell.z)) {
            ++i; // ground not loaded — hang like falling blocks do
            continue;
        }

        item.vel.y -= kGravity * kTickDt;
        bool onGround = false;
        if (cell.y >= 0 && cell.y < World::kHeightBlocks && m_world.IsSolid(cell.x, cell.y, cell.z)) {
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
        const bool crossedCell =
            glm::ivec3{glm::floor(item.prevPos)} != glm::ivec3{glm::floor(item.pos)};
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

void EntityManager::CrushDrops(const glm::ivec3& pos) {
    const auto& registry = BlockRegistry::Get();
    const BlockId id = m_world.GetBlock(pos.x, pos.y, pos.z);
    const BlockDef& def = registry.Def(id);
    if (def.cross || def.torch) {
        SpawnBlockDrop(pos, def.ResolveDrop(id), 1);
    }
}

bool EntityManager::FallingBlockVisible(const FallingBlock& falling) const {
    // Landed cubes cover the not-yet-remeshed block; in-flight cubes stay
    // hidden while the stale mesh still shows the block they came from.
    return falling.landed || m_world.MeshCaughtUp(falling.syncCell, falling.syncVersion);
}

} // namespace vc
