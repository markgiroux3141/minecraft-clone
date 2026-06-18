#include "entity/EntityManager.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "world/Block.h"
#include "world/World.h"

// M35 explosion system + primed TNT (split out of EntityManager.cpp to keep that
// file within budget). The block-removal ray, entity-damage falloff, and TNT
// physics are ported from the 1.12 source (world/Explosion.java,
// entity/item/EntityTNTPrimed.java) — see docs/HANDOFF.md.

namespace vc {

namespace {

// Gameplay-only randomness (ray jitter, drop rolls, TNT spawn kick) — nothing
// worldgen-deterministic flows through it, like World.cpp's ItemRand01.
uint32_t g_expRng = 0x2545f491u;
float ExpRand01() {
    g_expRng ^= g_expRng << 13;
    g_expRng ^= g_expRng >> 17;
    g_expRng ^= g_expRng << 5;
    return static_cast<float>(g_expRng & 0xFFFFFFu) / 16777215.0f;
}

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
               static_cast<size_t>(v.z) * 83492791u;
    }
};

constexpr float kTickDt = 0.05f; // 20 TPS

} // namespace

void EntityManager::SpawnPrimedTnt(const glm::vec3& pos, int fuse) {
    PrimedTnt tnt;
    tnt.pos = pos;
    tnt.prevPos = pos;
    // Vanilla EntityTNTPrimed: small random horizontal kick + a 0.2 b/tick
    // (4 b/s) upward pop so it hops off the block it was primed on.
    const float a = ExpRand01() * 6.2831853f;
    tnt.vel = {-std::sin(a) * 0.4f, 4.0f, -std::cos(a) * 0.4f};
    tnt.fuse = fuse;
    tnt.prevFuse = fuse;
    m_primedTnt.push_back(tnt);
}

void EntityManager::TickPrimedTnt() {
    constexpr float kGravity = 16.0f; // vanilla 0.04 b/tick^2
    constexpr float kHalf = 0.49f;    // 0.98-wide cube
    constexpr float kHeight = 0.98f;

    // Axis-separated AABB move against solid blocks, sized for the TNT cube
    // (mirrors the item-entity / player schemes). Returns true on collision.
    const auto moveAxis = [&](PrimedTnt& tnt, int axis, float delta) {
        if (delta == 0.0f) {
            return false;
        }
        tnt.pos[axis] += delta;
        const glm::vec3 boxMin = tnt.pos - glm::vec3{kHalf, 0.0f, kHalf};
        const glm::vec3 boxMax = tnt.pos + glm::vec3{kHalf, kHeight, kHalf};
        const glm::ivec3 lo{static_cast<int>(std::floor(boxMin.x + 1e-5f)),
                            static_cast<int>(std::floor(boxMin.y + 1e-5f)),
                            static_cast<int>(std::floor(boxMin.z + 1e-5f))};
        const glm::ivec3 hi{static_cast<int>(std::floor(boxMax.x - 1e-5f)),
                            static_cast<int>(std::floor(boxMax.y - 1e-5f)),
                            static_cast<int>(std::floor(boxMax.z - 1e-5f))};
        bool collided = false;
        float resolved = tnt.pos[axis];
        for (int by = lo.y; by <= hi.y; ++by) {
            for (int bz = lo.z; bz <= hi.z; ++bz) {
                for (int bx = lo.x; bx <= hi.x; ++bx) {
                    if (!m_world.IsSolid(bx, by, bz)) {
                        continue;
                    }
                    collided = true;
                    const int cell = (axis == 0) ? bx : (axis == 1) ? by : bz;
                    if (delta > 0.0f) {
                        const float extent = (axis == 1) ? kHeight : kHalf;
                        resolved = std::min(resolved, static_cast<float>(cell) - extent - 0.001f);
                    } else {
                        const float extent = (axis == 1) ? 0.0f : kHalf;
                        resolved =
                            std::max(resolved, static_cast<float>(cell + 1) + extent + 0.001f);
                    }
                }
            }
        }
        if (collided) {
            tnt.pos[axis] = resolved;
            tnt.vel[axis] = 0.0f;
        }
        return collided;
    };

    for (size_t i = 0; i < m_primedTnt.size();) {
        PrimedTnt& tnt = m_primedTnt[i];
        tnt.prevPos = tnt.pos;
        tnt.prevFuse = tnt.fuse;

        const glm::ivec3 cell{static_cast<int>(std::floor(tnt.pos.x)),
                              static_cast<int>(std::floor(tnt.pos.y + kHalf)),
                              static_cast<int>(std::floor(tnt.pos.z))};
        if (m_world.IsChunkLoaded(cell.x, std::clamp(cell.y, 0, World::kHeightBlocks - 1), cell.z)) {
            tnt.vel.y -= kGravity * kTickDt;
            const bool onGround = moveAxis(tnt, 1, tnt.vel.y * kTickDt) && tnt.prevPos.y >= tnt.pos.y;
            moveAxis(tnt, 0, tnt.vel.x * kTickDt);
            moveAxis(tnt, 2, tnt.vel.z * kTickDt);
            // Vanilla drag x0.98; on the ground horizontal x0.7, vertical bounce x-0.5.
            tnt.vel.x *= 0.98f;
            tnt.vel.y *= 0.98f;
            tnt.vel.z *= 0.98f;
            if (onGround) {
                tnt.vel.x *= 0.7f;
                tnt.vel.z *= 0.7f;
                tnt.vel.y *= -0.5f;
            }
        }

        if (--tnt.fuse <= 0) {
            const glm::vec3 center = tnt.pos + glm::vec3{0.0f, kHeight * 0.5f, 0.0f};
            m_primedTnt.erase(m_primedTnt.begin() + static_cast<ptrdiff_t>(i));
            Explode(center, 4.0f); // vanilla TNT radius
            continue;              // erased; indices past i shifted
        }
        ++i;
    }
}

float EntityManager::BlastDensity(const glm::vec3& center, const glm::vec3& target) const {
    const glm::vec3 d = target - center;
    const float dist = glm::length(d);
    if (dist < 1e-3f) {
        return 1.0f;
    }
    // Single ray center->target: clear line of sight = full exposure, an
    // intervening block = roughly the vanilla "behind cover" reduction.
    const auto hit = m_world.RaycastBlocks(center, d / dist, dist - 0.1f);
    return hit ? 0.4f : 1.0f;
}

void EntityManager::Explode(const glm::vec3& center, float size) {
    const auto& registry = BlockRegistry::Get();

    // --- doExplosionA: collect blocks the rays punch through ----------------
    std::unordered_set<glm::ivec3, IVec3Hash> destroyed;
    for (int j = 0; j < 16; ++j) {
        for (int k = 0; k < 16; ++k) {
            for (int l = 0; l < 16; ++l) {
                if (!(j == 0 || j == 15 || k == 0 || k == 15 || l == 0 || l == 15)) {
                    continue; // only the 16^3 cube's surface shell
                }
                glm::vec3 dir{static_cast<float>(j) / 15.0f * 2.0f - 1.0f,
                              static_cast<float>(k) / 15.0f * 2.0f - 1.0f,
                              static_cast<float>(l) / 15.0f * 2.0f - 1.0f};
                const float len = glm::length(dir);
                dir /= len;
                float strength = size * (0.7f + ExpRand01() * 0.6f);
                glm::vec3 p = center;
                for (; strength > 0.0f; strength -= 0.225f) {
                    const glm::ivec3 cell{static_cast<int>(std::floor(p.x)),
                                          static_cast<int>(std::floor(p.y)),
                                          static_cast<int>(std::floor(p.z))};
                    const BlockId id = m_world.GetBlock(cell.x, cell.y, cell.z);
                    if (id != blocks::Air) {
                        const BlockDef& def = registry.Def(id);
                        // Unbreakable (bedrock) + liquids absorb the ray entirely
                        // so they're never carved (vanilla: huge resistance).
                        const float res = (def.unbreakable || def.liquid) ? 1.0e9f
                                                                          : def.BlastResistance();
                        strength -= (res + 0.3f) * 0.3f;
                        if (strength > 0.0f && !def.unbreakable && !def.liquid) {
                            destroyed.insert(cell);
                        }
                    }
                    p += dir * 0.3f;
                }
            }
        }
    }

    // --- doExplosionA tail: damage + knock back entities --------------------
    // (Computed against the intact terrain, before block removal, like vanilla.)
    const float radius = size * 2.0f;
    if (m_damagePlayer) {
        const glm::vec3 playerCenter = m_playerFeet + glm::vec3{0.0f, 0.9f, 0.0f};
        const float dist = glm::length(playerCenter - center);
        if (dist <= radius && dist > 1e-3f) {
            const float density = BlastDensity(center, playerCenter);
            const float d = (1.0f - dist / radius) * density;
            const int dmg = static_cast<int>((d * d + d) / 2.0f * 7.0f * radius + 1.0f);
            if (dmg > 0) {
                m_damagePlayer(static_cast<float>(dmg), center);
            }
        }
    }
    for (size_t i = 0; i < m_mobs.size();) {
        const Mob& mob = m_mobs[i];
        const MobDef& def = MobDefOf(mob.type);
        const glm::vec3 mobCenter = mob.pos + glm::vec3{0.0f, def.height * 0.5f, 0.0f};
        const float dist = glm::length(mobCenter - center);
        if (dist <= radius && dist > 1e-3f) {
            const float density = BlastDensity(center, mobCenter);
            const float d = (1.0f - dist / radius) * density;
            const int dmg = static_cast<int>((d * d + d) / 2.0f * 7.0f * radius + 1.0f);
            if (dmg > 0) {
                const size_t before = m_mobs.size();
                DamageMob(i, static_cast<float>(dmg), center); // may erase on death
                if (m_mobs.size() < before) {
                    continue; // erased — don't advance
                }
            }
        }
        ++i;
    }

    // --- doExplosionB: remove blocks, spawn a fraction of their drops -------
    const float dropChance = 1.0f / size;
    for (const glm::ivec3& cell : destroyed) {
        const BlockId id = m_world.GetBlock(cell.x, cell.y, cell.z);
        if (id == blocks::Air) {
            continue;
        }
        const BlockDef& def = registry.Def(id);
        if (ExpRand01() < dropChance) {
            SpawnBlockDrop(cell, def.ResolveDrop(id), 1);
        }
        m_world.SetBlock(cell, blocks::Air); // schedules neighbor updates (sand/water)
    }

    m_explosions.push_back({center, size}); // GameApp plays the boom + smoke
}

} // namespace vc
