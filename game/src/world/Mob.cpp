#include "world/Mob.h"

#include <algorithm>
#include <cmath>

#include "world/Block.h"
#include "world/Light.h"
#include "world/World.h"

namespace vc {

ItemId MobDropItem(MobType type) {
    switch (type) {
    case MobType::Pig: return items::RawPorkchop;
    case MobType::Zombie: return items::RottenFlesh;
    default: return blocks::Air;
    }
}

namespace {

// Gameplay-only randomness (mob AI, spawning, scatter) — nothing
// worldgen-deterministic flows through it, like World.cpp's ItemRand01.
uint32_t g_mobRng = 0x1f123bb5u;
float MobRand01() {
    g_mobRng ^= g_mobRng << 13;
    g_mobRng ^= g_mobRng >> 17;
    g_mobRng ^= g_mobRng << 5;
    return static_cast<float>(g_mobRng & 0xFFFFFFu) / 16777215.0f;
}
int MobRandInt(int n) { return n <= 0 ? 0 : static_cast<int>(MobRand01() * static_cast<float>(n)) % n; }

constexpr float kTickDt = 0.05f; // 20 TPS, fixed (like FallingBlock)
constexpr float kGravity = 32.0f;
constexpr float kTerminal = 60.0f;
constexpr float kStepHeight = 1.0f; // mobs step a full block (vanilla default)
constexpr float kSkin = 0.001f;

// Single-axis AABB move + clamp against block collision boxes — the same
// axis-separated resolution as Player::MoveAxis, parameterized by the mob's
// half-width/height. Sets onGround when a downward Y move lands.
bool MoveMobAxis(const World& world, glm::vec3& pos, glm::vec3& vel, bool& onGround, int axis,
                 float delta, float half, float height) {
    if (delta == 0.0f) {
        return false;
    }
    pos[axis] += delta;

    const glm::vec3 boxMin = pos - glm::vec3{half, 0.0f, half};
    const glm::vec3 boxMax = pos + glm::vec3{half, height, half};
    const glm::ivec3 lo{static_cast<int>(std::floor(boxMin.x + 1e-5f)),
                        static_cast<int>(std::floor(boxMin.y + 1e-5f)),
                        static_cast<int>(std::floor(boxMin.z + 1e-5f))};
    const glm::ivec3 hi{static_cast<int>(std::floor(boxMax.x - 1e-5f)),
                        static_cast<int>(std::floor(boxMax.y - 1e-5f)),
                        static_cast<int>(std::floor(boxMax.z - 1e-5f))};

    const int b = (axis + 1) % 3;
    const int c = (axis + 2) % 3;
    const float extentPos = (axis == 1) ? height : half;
    const float extentNeg = (axis == 1) ? 0.0f : half;
    const float preLead = (pos[axis] - delta) + (delta > 0.0f ? extentPos : -extentNeg);

    bool collided = false;
    float resolved = pos[axis];
    for (int by = lo.y; by <= hi.y; ++by) {
        for (int bz = lo.z; bz <= hi.z; ++bz) {
            for (int bx = lo.x; bx <= hi.x; ++bx) {
                World::BlockBox boxes[2];
                const int n = world.CollisionBoxesAt(bx, by, bz, boxes);
                const glm::vec3 cell{static_cast<float>(bx), static_cast<float>(by),
                                     static_cast<float>(bz)};
                for (int k = 0; k < n; ++k) {
                    const glm::vec3 bMin = cell + boxes[k].min;
                    const glm::vec3 bMax = cell + boxes[k].max;
                    if (boxMin[b] >= bMax[b] || boxMax[b] <= bMin[b] || boxMin[c] >= bMax[c] ||
                        boxMax[c] <= bMin[c]) {
                        continue;
                    }
                    if (delta > 0.0f) {
                        if (bMin[axis] < preLead - kSkin) {
                            continue;
                        }
                        collided = true;
                        resolved = std::min(resolved, bMin[axis] - extentPos - kSkin);
                    } else {
                        if (bMax[axis] > preLead + kSkin) {
                            continue;
                        }
                        collided = true;
                        resolved = std::max(resolved, bMax[axis] + extentNeg + kSkin);
                    }
                }
            }
        }
    }

    if (collided) {
        pos[axis] = resolved;
        vel[axis] = 0.0f;
        if (axis == 1 && delta < 0.0f) {
            onGround = true;
        }
    }
    return collided;
}

} // namespace

void World::SpawnMob(MobType type, const glm::vec3& feetPos) {
    Mob mob;
    mob.type = type;
    mob.pos = feetPos;
    mob.prevPos = feetPos;
    mob.health = MobDefOf(type).maxHealth;
    mob.aiTimer = MobRandInt(40);
    m_mobs.push_back(mob);
}

void World::EmitMobDeath(const Mob& mob) {
    const MobDef& def = MobDefOf(mob.type);
    const ItemId drop = MobDropItem(mob.type);
    const int count = def.dropMin + MobRandInt(def.dropMax - def.dropMin + 1);
    if (drop != blocks::Air && count > 0) {
        const glm::ivec3 cell{static_cast<int>(std::floor(mob.pos.x)),
                              static_cast<int>(std::floor(mob.pos.y + def.height * 0.5f)),
                              static_cast<int>(std::floor(mob.pos.z))};
        SpawnBlockDrop(cell, drop, count);
    }
    m_mobSounds.push_back({mob.type, mob.pos, 1}); // death
}

void World::SaveMobs() {
    std::vector<WorldSave::MobRecord> records;
    records.reserve(m_mobs.size());
    for (const Mob& m : m_mobs) {
        records.push_back({static_cast<int>(m.type), m.pos, m.yaw, m.health});
    }
    m_save.SetMobs(std::move(records));
}

std::optional<size_t> World::RaycastMob(const glm::vec3& origin, const glm::vec3& dir,
                                        float maxDist, float& outDist) const {
    // Slab method against each mob's AABB; nearest entry distance wins.
    std::optional<size_t> best;
    float bestT = maxDist;
    for (size_t i = 0; i < m_mobs.size(); ++i) {
        const Mob& m = m_mobs[i];
        const MobDef& def = MobDefOf(m.type);
        const glm::vec3 bMin = m.pos - glm::vec3{def.halfWidth, 0.0f, def.halfWidth};
        const glm::vec3 bMax = m.pos + glm::vec3{def.halfWidth, def.height, def.halfWidth};
        float tNear = 0.0f, tFar = bestT;
        bool hit = true;
        for (int a = 0; a < 3; ++a) {
            if (std::abs(dir[a]) < 1e-8f) {
                if (origin[a] < bMin[a] || origin[a] > bMax[a]) {
                    hit = false;
                    break;
                }
                continue;
            }
            float t0 = (bMin[a] - origin[a]) / dir[a];
            float t1 = (bMax[a] - origin[a]) / dir[a];
            if (t0 > t1) {
                std::swap(t0, t1);
            }
            tNear = std::max(tNear, t0);
            tFar = std::min(tFar, t1);
            if (tNear > tFar) {
                hit = false;
                break;
            }
        }
        if (hit && tNear < bestT) {
            bestT = tNear;
            best = i;
        }
    }
    if (best) {
        outDist = bestT;
    }
    return best;
}

void World::DamageMob(size_t index, float amount, const glm::vec3& fromPos) {
    if (index >= m_mobs.size() || amount <= 0.0f) {
        return;
    }
    Mob& mob = m_mobs[index];
    mob.health -= amount;
    mob.hurtTime = 10;
    m_mobSounds.push_back({mob.type, mob.pos, 0}); // hurt

    // Knockback away from the source (horizontal + a little up).
    glm::vec3 away = mob.pos - fromPos;
    away.y = 0.0f;
    if (glm::length(away) > 1e-4f) {
        away = glm::normalize(away);
        mob.vel.x = away.x * 8.0f;
        mob.vel.z = away.z * 8.0f;
    }
    mob.vel.y = 7.0f;

    if (mob.health <= 0.0f) {
        EmitMobDeath(mob);
        m_mobs.erase(m_mobs.begin() + static_cast<ptrdiff_t>(index));
    }
}

void World::TickMobs(const MobTickCtx& ctx) {
    const glm::vec3 playerFeet = ctx.playerFeet;

    for (size_t i = 0; i < m_mobs.size();) {
        Mob& mob = m_mobs[i];
        const MobDef& def = MobDefOf(mob.type);
        mob.prevPos = mob.pos;
        mob.prevYaw = mob.yaw;
        if (mob.hurtTime > 0) {
            --mob.hurtTime;
        }
        if (mob.attackCooldown > 0) {
            --mob.attackCooldown;
        }
        if (mob.aiTimer > 0) {
            --mob.aiTimer;
        }

        // Freeze while the chunk underfoot has no data (don't fall through
        // still-generating terrain) — exactly like the player / falling sand.
        const glm::ivec3 feetChunk{static_cast<int>(std::floor(mob.pos.x)) >> 4,
                                   std::clamp(static_cast<int>(std::floor(mob.pos.y)) >> 4, 0,
                                              kHeightChunks - 1),
                                   static_cast<int>(std::floor(mob.pos.z)) >> 4};
        if (!GetChunk(feetChunk)) {
            ++i;
            continue;
        }

        // --- AI: pick a horizontal wish direction + facing ----------------
        glm::vec2 wish{0.0f};
        mob.moving = false;
        const glm::vec3 toPlayer = playerFeet - mob.pos;
        const float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        bool chasing = false;
        if (def.hostile && distXZ <= def.followRange) {
            // Walk straight at the player (simple ground pathing; auto-step
            // handles 1-block rises).
            chasing = true;
            mob.moving = true;
            wish = glm::normalize(glm::vec2{toPlayer.x, toPlayer.z});
            mob.yaw = std::atan2(toPlayer.z, toPlayer.x);
        } else {
            // Idle / wander: every so often start a stroll in a random
            // direction, otherwise stand still.
            if (mob.aiTimer <= 0) {
                if (MobRand01() < 0.5f) {
                    const float a = MobRand01() * 6.2831853f;
                    mob.wanderDir = {std::cos(a), std::sin(a)};
                    mob.aiTimer = 40 + MobRandInt(60); // 2..5 s stroll
                } else {
                    mob.wanderDir = {0.0f, 0.0f};
                    mob.aiTimer = 20 + MobRandInt(60); // 1..4 s pause
                }
            }
            if (mob.wanderDir != glm::vec2{0.0f}) {
                mob.moving = true;
                wish = mob.wanderDir;
                mob.yaw = std::atan2(wish.y, wish.x);
            }
        }

        // --- Physics: gravity + axis-separated collision with auto-step ----
        const float speed = chasing ? def.speed : def.speed * 0.55f;
        mob.vel.y = std::max(mob.vel.y - kGravity * kTickDt, -kTerminal);

        // Horizontal movement with an acceleration ramp: step the current
        // velocity toward the target (wish*speed, or 0 when idle) by a capped
        // amount each tick instead of snapping to it. This gives mobs momentum
        // and reaction lag — they spend a few ticks getting up to speed and
        // can't instantly reverse to track a dodging player. A knockback impulse
        // (set high by DamageMob) bleeds off through the same ramp, so it needs
        // no special case.
        const glm::vec2 target = mob.moving ? wish * speed : glm::vec2{0.0f}; // wish is (x,z)
        glm::vec2 cur{mob.vel.x, mob.vel.z};
        glm::vec2 dv = target - cur;
        constexpr float kAccel = 14.0f; // b/s^2 (reaches ~4 b/s in ~0.3 s)
        const float maxStep = kAccel * kTickDt;
        const float dvLen = std::sqrt(dv.x * dv.x + dv.y * dv.y);
        if (dvLen > maxStep && dvLen > 1e-6f) {
            dv *= maxStep / dvLen;
        }
        cur += dv;
        mob.vel.x = cur.x;
        mob.vel.z = cur.y;

        const float startY = mob.pos.y;
        mob.onGround = false;
        MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 1, mob.vel.y * kTickDt, def.halfWidth,
                    def.height);
        const bool groundedBeforeStep = mob.onGround;

        const glm::vec3 flatStart = mob.pos;
        const glm::vec3 flatVel = mob.vel;
        const bool hitX = MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 0,
                                      mob.vel.x * kTickDt, def.halfWidth, def.height);
        const bool hitZ = MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 2,
                                      mob.vel.z * kTickDt, def.halfWidth, def.height);
        if ((hitX || hitZ) && groundedBeforeStep) {
            // Try the same move lifted by a step and settled back (walk up a
            // block); keep it only if it actually advanced (not a full wall).
            const glm::vec3 flatEnd = mob.pos;
            const glm::vec3 flatEndVel = mob.vel;
            mob.pos = flatStart;
            mob.vel = flatVel;
            const float beforeLift = mob.pos.y;
            MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 1, kStepHeight, def.halfWidth,
                        def.height);
            const float lifted = mob.pos.y - beforeLift;
            MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 0, flatVel.x * kTickDt,
                        def.halfWidth, def.height);
            MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 2, flatVel.z * kTickDt,
                        def.halfWidth, def.height);
            MoveMobAxis(*this, mob.pos, mob.vel, mob.onGround, 1, -lifted, def.halfWidth,
                        def.height);
            const float stepGain = (mob.pos.x - flatStart.x) * (mob.pos.x - flatStart.x) +
                                   (mob.pos.z - flatStart.z) * (mob.pos.z - flatStart.z);
            const float flatGain = (flatEnd.x - flatStart.x) * (flatEnd.x - flatStart.x) +
                                   (flatEnd.z - flatStart.z) * (flatEnd.z - flatStart.z);
            if (stepGain <= flatGain + 1e-6f) {
                mob.pos = flatEnd;
                mob.vel = flatEndVel;
                mob.onGround = true;
            }
        }

        // --- Walk-cycle animation accumulators (vanilla EntityLivingBase) ---
        const float dx = mob.pos.x - mob.prevPos.x;
        const float dz = mob.pos.z - mob.prevPos.z;
        float animSpeed = std::sqrt(dx * dx + dz * dz) * 4.0f;
        if (animSpeed > 1.0f) {
            animSpeed = 1.0f;
        }
        mob.prevLimbSwing = mob.limbSwing;
        mob.prevLimbSwingAmount = mob.limbSwingAmount;
        mob.limbSwingAmount += (animSpeed - mob.limbSwingAmount) * 0.4f;
        mob.limbSwing += mob.limbSwingAmount;
        mob.age += 1.0f;

        // --- Soft entity push: shove out of the player's body -------------
        {
            const glm::vec3 d = mob.pos - playerFeet;
            const float ox = (def.halfWidth + ctx.playerHalfWidth) - std::abs(d.x);
            const float oz = (def.halfWidth + ctx.playerHalfWidth) - std::abs(d.z);
            const bool vert = mob.pos.y < playerFeet.y + ctx.playerHeight &&
                              mob.pos.y + def.height > playerFeet.y;
            if (vert && ox > 0.0f && oz > 0.0f) {
                if (ox < oz) {
                    const float s = (d.x >= 0.0f ? 0.05f : -0.05f);
                    mob.pos.x += s;
                    if (ctx.pushPlayer) {
                        ctx.pushPlayer(-s, 0.0f);
                    }
                } else {
                    const float s = (d.z >= 0.0f ? 0.05f : -0.05f);
                    mob.pos.z += s;
                    if (ctx.pushPlayer) {
                        ctx.pushPlayer(0.0f, -s);
                    }
                }
            }
        }

        // --- Hostile melee -------------------------------------------------
        if (chasing && mob.attackCooldown <= 0 &&
            distXZ <= def.halfWidth + ctx.playerHalfWidth + 0.7f &&
            std::abs(toPlayer.y) < 2.0f && ctx.damagePlayer) {
            ctx.damagePlayer(def.attackDamage, mob.pos);
            mob.attackCooldown = 20; // ~1 s between swings
        }

        // --- Fall damage / death ------------------------------------------
        if (mob.onGround) {
            if (mob.fallDistance > 3.0f) {
                mob.health -= std::ceil(mob.fallDistance - 3.0f);
                mob.hurtTime = 10;
                m_mobSounds.push_back({mob.type, mob.pos, 0});
            }
            mob.fallDistance = 0.0f;
        } else if (mob.pos.y < startY) {
            mob.fallDistance += startY - mob.pos.y;
        }
        if (mob.pos.y < -64.0f) {
            mob.health = 0.0f; // void
        }
        if (mob.health <= 0.0f) {
            EmitMobDeath(mob);
            m_mobs.erase(m_mobs.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }

        // --- Despawn far-away hostiles (passives persist) ------------------
        if (def.hostile && distXZ > 54.0f) {
            m_mobs.erase(m_mobs.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }
        ++i;
    }

    // Periodic natural spawn attempt (~ every 2 s).
    if (++m_mobTickCount % 40 == 0) {
        SpawnMobs(ctx);
    }
}

void World::SpawnMobs(const MobTickCtx& ctx) {
    // Category caps within a generous radius of the player.
    int passive = 0;
    int hostile = 0;
    for (const Mob& m : m_mobs) {
        const glm::vec3 d = m.pos - ctx.playerFeet;
        if (d.x * d.x + d.z * d.z <= 64.0f * 64.0f) {
            (MobDefOf(m.type).hostile ? hostile : passive) += 1;
        }
    }
    constexpr int kPassiveCap = 8;
    constexpr int kHostileCap = 8;

    // A few attempts; each picks a ring position, finds the ground, and checks
    // the light/surface rules for the appropriate category.
    for (int attempt = 0; attempt < 6; ++attempt) {
        const float angle = MobRand01() * 6.2831853f;
        const float radius = 24.0f + MobRand01() * 20.0f; // 24..44 blocks out
        const int wx = static_cast<int>(std::floor(ctx.playerFeet.x + std::cos(angle) * radius));
        const int wz = static_cast<int>(std::floor(ctx.playerFeet.z + std::sin(angle) * radius));

        // Find the ground column near the player's height (only if loaded).
        const int top = static_cast<int>(std::floor(ctx.playerFeet.y)) + 12;
        const int bottom = std::max(1, static_cast<int>(std::floor(ctx.playerFeet.y)) - 12);
        int groundY = -1;
        for (int y = top; y >= bottom; --y) {
            const BlockDef& below = BlockRegistry::Get().Def(GetBlock(wx, y - 1, wz));
            const bool airHere = !BlockRegistry::Get().Def(GetBlock(wx, y, wz)).solid;
            const bool airAbove = !BlockRegistry::Get().Def(GetBlock(wx, y + 1, wz)).solid;
            if (below.solid && !below.liquid && !below.cross && airHere && airAbove) {
                groundY = y;
                break;
            }
        }
        if (groundY < 0) {
            continue; // no valid ground (or chunk not loaded)
        }

        const uint8_t packed = PackedLightAt({wx, groundY, wz});
        const int skyLight = ChunkLight::Sky(packed);
        const int blockLight = ChunkLight::Block(packed);
        const BlockId belowId = GetBlock(wx, groundY - 1, wz);

        // Pick the category by the spawn rules; bail if its cap is full.
        MobType type;
        if (belowId == blocks::Grass && skyLight >= 9 && !ctx.isNight) {
            if (passive >= kPassiveCap) {
                continue;
            }
            type = MobType::Pig;
        } else if (blockLight <= 7 && (ctx.isNight || skyLight <= 7)) {
            if (hostile >= kHostileCap) {
                continue;
            }
            type = MobType::Zombie;
        } else {
            continue;
        }

        SpawnMob(type, {static_cast<float>(wx) + 0.5f, static_cast<float>(groundY),
                        static_cast<float>(wz) + 0.5f});
        return; // one spawn per sweep
    }
}

} // namespace vc
