#include "entity/EntityManager.h"

#include <algorithm>
#include <cmath>

#include "world/Block.h"
#include "world/World.h"

// M36 projectile system (arrows). Split out of EntityManager.cpp like
// Explosion.cpp to keep that file within budget. Physics + collision are ported
// from the 1.12 source (entity/projectile/EntityArrow.java): gravity 0.05
// b/tick^2, drag x0.99 (x0.6 submerged), and a per-tick swept test of the
// segment travelled — block hit -> stick, entity hit -> damage scaled by impact
// speed. See docs/HANDOFF.md.

namespace vc {

namespace {

constexpr float kTickDt = 0.05f;          // 20 TPS
constexpr float kArrowGravity = 20.0f;    // vanilla 0.05 b/tick^2 -> b/s^2
constexpr int kArrowDespawnAge = 1200;    // vanilla EntityArrow (60 s)

// Slab test: does the segment p0 -> p0+delta cross the AABB [min,max]?
// Returns the entry parameter t in [0,1] via outT when it does.
bool SegmentHitsBox(const glm::vec3& p0, const glm::vec3& delta, const glm::vec3& boxMin,
                    const glm::vec3& boxMax, float& outT) {
    float tNear = 0.0f, tFar = 1.0f;
    for (int a = 0; a < 3; ++a) {
        if (std::abs(delta[a]) < 1e-8f) {
            if (p0[a] < boxMin[a] || p0[a] > boxMax[a]) {
                return false;
            }
            continue;
        }
        float t0 = (boxMin[a] - p0[a]) / delta[a];
        float t1 = (boxMax[a] - p0[a]) / delta[a];
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tNear = std::max(tNear, t0);
        tFar = std::min(tFar, t1);
        if (tNear > tFar) {
            return false;
        }
    }
    outT = tNear;
    return true;
}

} // namespace

void EntityManager::SpawnArrow(const glm::vec3& pos, const glm::vec3& velocity, ArrowOwner owner,
                               float damage, bool playerPickup) {
    Arrow a;
    a.pos = pos;
    a.prevPos = pos;
    a.vel = velocity;
    a.owner = owner;
    a.damage = damage;
    a.playerPickup = playerPickup;
    // Vanilla EntityArrow heading convention (rotationYaw = atan2(mX, mZ)) so the
    // render transform (rotate yaw-90 about Y, pitch about Z) ports verbatim.
    const float horiz = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    a.yaw = std::atan2(velocity.x, velocity.z);
    a.pitch = std::atan2(velocity.y, horiz);
    a.prevYaw = a.yaw;
    a.prevPitch = a.pitch;
    m_arrows.push_back(a);
}

void EntityManager::TickArrows() {
    const auto& registry = BlockRegistry::Get();
    for (size_t i = 0; i < m_arrows.size();) {
        Arrow& a = m_arrows[i];
        a.BeginTickArrow();
        if (++a.life > kArrowDespawnAge) {
            m_arrows.erase(m_arrows.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }
        if (a.stuck) {
            ++i; // lodged — just age out (or get collected via PickupArrows)
            continue;
        }

        // Freeze while the chunk it sits in has no data (don't tunnel through
        // ungenerated terrain) — like the player / falling sand / TNT.
        const glm::ivec3 cell{static_cast<int>(std::floor(a.pos.x)),
                              static_cast<int>(std::floor(a.pos.y)),
                              static_cast<int>(std::floor(a.pos.z))};
        if (!m_world.IsChunkLoaded(cell.x, std::clamp(cell.y, 0, World::kHeightBlocks - 1),
                                   cell.z)) {
            ++i;
            continue;
        }

        // Gravity + per-tick drag (x0.6 submerged, x0.99 in air).
        a.vel.y -= kArrowGravity * kTickDt;
        const bool inLiquid = registry.Def(m_world.GetBlock(cell.x, cell.y, cell.z)).liquid;
        a.vel *= inLiquid ? 0.6f : 0.99f;

        const glm::vec3 start = a.pos;
        const glm::vec3 delta = a.vel * kTickDt;
        const float len = glm::length(delta);
        if (len < 1e-6f) {
            ++i;
            continue;
        }
        const glm::vec3 dir = delta / len;

        // Entity hit along the swept segment, computed before the block hit so
        // an arrow grazing a mob in front of a wall still connects. The impact
        // damage is vanilla's ceil(speedPerTick * baseDamage).
        const float speedPerTick = len; // blocks travelled this tick
        const int impact = std::max(1, static_cast<int>(std::ceil(speedPerTick * a.damage)));
        bool consumed = false;
        if (a.owner == ArrowOwner::Player) {
            float t = 0.0f;
            if (const auto idx = RaycastMob(start, dir, len, t)) {
                DamageMob(*idx, static_cast<float>(impact), start);
                consumed = true;
            }
        } else { // Mob arrow -> the player
            const glm::vec3 pMin = m_playerFeet - glm::vec3{m_playerHalfWidth, 0.0f, m_playerHalfWidth};
            const glm::vec3 pMax = m_playerFeet + glm::vec3{m_playerHalfWidth, m_playerHeight,
                                                            m_playerHalfWidth};
            float t = 0.0f;
            if (SegmentHitsBox(start, delta, pMin, pMax, t) && m_damagePlayer) {
                // Vanilla un-enchanted arrows barely knock back (no Punch) — a
                // light shove so a kiting skeleton stays catchable.
                m_damagePlayer(static_cast<float>(impact), start, 0.25f);
                consumed = true;
            }
        }
        if (consumed) {
            m_arrows.erase(m_arrows.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }

        // Block hit: stick into the first solid surface along the segment.
        if (const auto hit = m_world.RaycastBlocks(start, dir, len)) {
            a.pos = hit->point - dir * 0.05f; // back off a hair so it isn't inside
            a.vel = glm::vec3{0.0f};
            a.stuck = true;
            ++i;
            continue;
        }

        a.pos = start + delta;
        const float horiz = std::sqrt(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
        a.yaw = std::atan2(a.vel.x, a.vel.z);
        a.pitch = std::atan2(a.vel.y, horiz);
        ++i;
    }
}

} // namespace vc
