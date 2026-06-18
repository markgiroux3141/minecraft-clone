#include "entity/Mob.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "entity/EntityManager.h"
#include "world/Block.h"
#include "world/Light.h"
#include "world/World.h"

namespace vc {

std::vector<MobDrop> MobDrops(MobType type, bool sheared) {
    switch (type) {
    case MobType::Pig: return {{items::RawPorkchop, 1, 3}};
    case MobType::Zombie: return {{items::RottenFlesh, 0, 2}};
    case MobType::Cow: return {{items::RawBeef, 1, 3}, {items::LeatherItem, 0, 2}};
    case MobType::Sheep: {
        std::vector<MobDrop> drops = {{items::RawMutton, 1, 2}};
        if (!sheared) {
            drops.push_back({blocks::WhiteWool, 1, 1}); // wool block id (block half of the id space)
        }
        return drops;
    }
    case MobType::Chicken: return {{items::RawChicken, 1, 1}, {items::Feather, 0, 2}};
    case MobType::Creeper: return {{items::Gunpowder, 0, 2}}; // only if killed before it blows
    case MobType::Skeleton: return {{items::Arrow, 0, 2}, {items::Bone, 0, 2}};
    default: return {};
    }
}

const char* MobSoundFolder(MobType type) {
    switch (type) {
    case MobType::Pig: return "pig";
    case MobType::Zombie: return "zombie";
    case MobType::Cow: return "cow";
    case MobType::Sheep: return "sheep";
    case MobType::Chicken: return "chicken";
    case MobType::Creeper: return "creeper";
    case MobType::Skeleton: return "skeleton";
    default: return "pig";
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

void EntityManager::SpawnMob(MobType type, const glm::vec3& feetPos) {
    Mob mob;
    mob.type = type;
    mob.pos = feetPos;
    mob.prevPos = feetPos;
    mob.health = MobDefOf(type).maxHealth;
    mob.aiTimer = MobRandInt(40);
    if (MobDefOf(type).laysEggs) {
        mob.eggTimer = 6000 + MobRandInt(6000); // vanilla EntityChicken: 6000..11999
    }
    m_mobs.push_back(mob);
}

void EntityManager::EmitMobDeath(const Mob& mob) {
    const MobDef& def = MobDefOf(mob.type);
    const glm::ivec3 cell{static_cast<int>(std::floor(mob.pos.x)),
                          static_cast<int>(std::floor(mob.pos.y + def.height * 0.5f)),
                          static_cast<int>(std::floor(mob.pos.z))};
    for (const MobDrop& drop : MobDrops(mob.type, mob.sheared)) {
        const int count = drop.min + MobRandInt(drop.max - drop.min + 1);
        if (drop.item != blocks::Air && count > 0) {
            SpawnBlockDrop(cell, drop.item, count);
        }
    }
    m_mobSounds.push_back({mob.type, mob.pos, 1}); // death
}

void EntityManager::LoadMobs() {
    // Mobs saved with the world (M32). Unknown types from a newer build are
    // dropped rather than crashing.
    for (const auto& r : m_world.SaveStore().GetMobs()) {
        if (r.type < 0 || r.type >= static_cast<int>(MobType::Count)) {
            continue;
        }
        Mob mob;
        mob.type = static_cast<MobType>(r.type);
        mob.pos = r.pos;
        mob.prevPos = r.pos;
        mob.yaw = r.yaw;
        mob.prevYaw = r.yaw;
        mob.health = std::clamp(r.health, 0.0f, MobDefOf(mob.type).maxHealth);
        if (MobDefOf(mob.type).laysEggs) {
            mob.eggTimer = 6000 + MobRandInt(6000); // egg cycle restarts on load
        }
        if (mob.health > 0.0f) {
            m_mobs.push_back(mob);
        }
    }
}

void EntityManager::SaveMobs() {
    std::vector<WorldSave::MobRecord> records;
    records.reserve(m_mobs.size());
    for (const Mob& m : m_mobs) {
        records.push_back({static_cast<int>(m.type), m.pos, m.yaw, m.health});
    }
    m_world.SaveStore().SetMobs(std::move(records));
}

std::optional<size_t> EntityManager::RaycastMob(const glm::vec3& origin, const glm::vec3& dir,
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

void EntityManager::DamageMob(size_t index, float amount, const glm::vec3& fromPos) {
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

int EntityManager::ShearMob(size_t index) {
    if (index >= m_mobs.size()) {
        return 0;
    }
    Mob& mob = m_mobs[index];
    if (mob.type != MobType::Sheep || mob.sheared) {
        return 0;
    }
    mob.sheared = true;
    return 1 + MobRandInt(3); // vanilla EntitySheep.onSheared: 1..3 wool
}

bool EntityManager::IgniteMob(size_t index) {
    if (index >= m_mobs.size()) {
        return false;
    }
    Mob& mob = m_mobs[index];
    if (MobDefOf(mob.type).explodeRadius <= 0.0f) {
        return false; // not a creeper -> RMB falls through
    }
    mob.fuseLit = true; // swells to detonation regardless of range now
    return true;
}

void EntityManager::TickMobs(const MobTickCtx& ctx) {
    const glm::vec3 playerFeet = ctx.playerFeet;

    // Creeper detonations are deferred to after the loop: Explode() erases dead
    // mobs (DamageMob), which would invalidate this loop's index. {center, radius}.
    std::vector<std::pair<glm::vec3, float>> pendingExplosions;

    for (size_t i = 0; i < m_mobs.size();) {
        Mob& mob = m_mobs[i];
        const MobDef& def = MobDefOf(mob.type);
        mob.prevPos = mob.pos;
        mob.prevYaw = mob.yaw;
        mob.prevFuse = mob.fuse;
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
                                              World::kHeightChunks - 1),
                                   static_cast<int>(std::floor(mob.pos.z)) >> 4};
        if (!m_world.GetChunk(feetChunk)) {
            ++i;
            continue;
        }

        // --- AI: pick a horizontal wish direction + facing ----------------
        glm::vec2 wish{0.0f};
        mob.moving = false;
        const glm::vec3 toPlayer = playerFeet - mob.pos;
        const float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        bool chasing = false;
        bool rangedRetreat = false; // skeleton backpedalling (slower than its approach)
        mob.aiming = false;
        if (def.ranged && distXZ <= def.followRange && distXZ > 1e-3f) {
            // M36 skeleton: keep distance + shoot. Face the player, approach to
            // within bowRange, back away when much closer, hold otherwise; fire a
            // player-targeting arrow every shootInterval ticks when there's a
            // clear line of sight (vanilla EntityAIAttackRangedBow).
            chasing = true; // chase speed for the approach
            const glm::vec2 toXZ = glm::normalize(glm::vec2{toPlayer.x, toPlayer.z});
            mob.yaw = std::atan2(toPlayer.z, toPlayer.x);
            // Vanilla EntityAIAttackRangedBow: approach when beyond bow range,
            // only back away when the player gets very close (~25% of range),
            // hold/strafe otherwise. Backpedal at a reduced speed so a walking
            // player (4.3 b/s vs the skeleton's 3.4) can run it down.
            if (distXZ > def.bowRange) {
                mob.moving = true;
                wish = toXZ;
            } else if (distXZ < def.bowRange * 0.3f) {
                mob.moving = true;
                wish = -toXZ;
                rangedRetreat = true;
            }
            const glm::vec3 eye = mob.pos + glm::vec3{0.0f, def.height * 0.9f, 0.0f};
            const glm::vec3 aim =
                playerFeet + glm::vec3{0.0f, ctx.playerHeight * 0.33f, 0.0f};
            const glm::vec3 los = aim - eye;
            const float losLen = glm::length(los);
            const bool clear =
                losLen < 1e-3f || !m_world.RaycastBlocks(eye, los / losLen, losLen).has_value();
            mob.aiming = clear;
            if (mob.shootCooldown > 0) {
                --mob.shootCooldown;
            }
            if (clear && mob.shootCooldown <= 0) {
                ShootArrowAt(eye, aim);
                mob.shootCooldown = def.shootInterval;
            }
        } else if (def.hostile && distXZ <= def.followRange) {
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
        const float speed = rangedRetreat ? def.speed * 0.6f
                            : chasing      ? def.speed
                                           : def.speed * 0.55f;
        mob.vel.y = std::max(mob.vel.y - kGravity * kTickDt, -kTerminal);
        // Chicken flutter: damp the descent to 60%/tick while airborne (vanilla
        // EntityChicken.onLivingUpdate) so it drifts down instead of plummeting.
        if (def.slowFall && !mob.onGround && mob.vel.y < 0.0f) {
            mob.vel.y *= 0.6f;
        }

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
        MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 1, mob.vel.y * kTickDt, def.halfWidth,
                    def.height);
        const bool groundedBeforeStep = mob.onGround;

        const glm::vec3 flatStart = mob.pos;
        const glm::vec3 flatVel = mob.vel;
        const bool hitX = MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 0,
                                      mob.vel.x * kTickDt, def.halfWidth, def.height);
        const bool hitZ = MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 2,
                                      mob.vel.z * kTickDt, def.halfWidth, def.height);
        if ((hitX || hitZ) && groundedBeforeStep) {
            // Try the same move lifted by a step and settled back (walk up a
            // block); keep it only if it actually advanced (not a full wall).
            const glm::vec3 flatEnd = mob.pos;
            const glm::vec3 flatEndVel = mob.vel;
            mob.pos = flatStart;
            mob.vel = flatVel;
            const float beforeLift = mob.pos.y;
            MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 1, kStepHeight, def.halfWidth,
                        def.height);
            const float lifted = mob.pos.y - beforeLift;
            MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 0, flatVel.x * kTickDt,
                        def.halfWidth, def.height);
            MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 2, flatVel.z * kTickDt,
                        def.halfWidth, def.height);
            MoveMobAxis(m_world, mob.pos, mob.vel, mob.onGround, 1, -lifted, def.halfWidth,
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

        // --- Hostile melee (biting mobs only; creeper deals 0 + explodes) --
        if (def.attackDamage > 0.0f && chasing && mob.attackCooldown <= 0 &&
            distXZ <= def.halfWidth + ctx.playerHalfWidth + 0.7f &&
            std::abs(toPlayer.y) < 2.0f && ctx.damagePlayer) {
            ctx.damagePlayer(def.attackDamage, mob.pos, 1.0f); // full melee knockback
            mob.attackCooldown = 20; // ~1 s between swings
        }

        // --- Creeper swell / fuse (M35) ------------------------------------
        // Vanilla EntityCreeper + AICreeperSwell: swell (fuse counts up) while
        // force-ignited OR within ~3 blocks of the player, otherwise the fuse
        // recedes. A 0->swelling edge plays the prime hiss. At fuseTime it
        // detonates — deferred past the loop (Explode erases mobs). It consumes
        // itself, so NO death drop (gunpowder only drops from a kill below).
        if (def.explodeRadius > 0.0f) {
            const bool inSwellRange = glm::length(toPlayer) <= 3.0f;
            if (mob.fuseLit || inSwellRange) {
                if (mob.fuse == 0) {
                    m_mobSounds.push_back({mob.type, mob.pos, 3}); // primed hiss
                }
                ++mob.fuse;
            } else if (mob.fuse > 0) {
                --mob.fuse;
            }
            if (mob.fuse >= def.fuseTime) {
                pendingExplosions.push_back(
                    {mob.pos + glm::vec3{0.0f, def.height * 0.5f, 0.0f}, def.explodeRadius});
                m_mobs.erase(m_mobs.begin() + static_cast<ptrdiff_t>(i));
                continue; // gone — no death drop
            }
        }

        // --- Egg laying (chicken) -----------------------------------------
        // Vanilla EntityChicken: every 6000..11999 ticks an adult drops an egg
        // at its feet and queues the "plop". No baby/jockey system, so every
        // chicken qualifies.
        if (def.laysEggs && --mob.eggTimer <= 0) {
            const glm::ivec3 cell{static_cast<int>(std::floor(mob.pos.x)),
                                  static_cast<int>(std::floor(mob.pos.y)),
                                  static_cast<int>(std::floor(mob.pos.z))};
            SpawnBlockDrop(cell, items::Egg, 1);
            m_mobSounds.push_back({mob.type, mob.pos, 2}); // egg plop
            mob.eggTimer = 6000 + MobRandInt(6000);
        }

        // --- Fall damage / death ------------------------------------------
        if (mob.onGround) {
            if (mob.fallDistance > 3.0f && !def.slowFall) {
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

    // Creeper detonations, now that the iteration is done (Explode erases mobs).
    for (const auto& [center, radius] : pendingExplosions) {
        Explode(center, radius);
    }

    // Periodic natural spawn attempt (~ every 2 s).
    if (++m_mobTickCount % 40 == 0) {
        SpawnMobs(ctx);
    }
}

void EntityManager::SpawnMobs(const MobTickCtx& ctx) {
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
            const BlockDef& below = BlockRegistry::Get().Def(m_world.GetBlock(wx, y - 1, wz));
            const bool airHere = !BlockRegistry::Get().Def(m_world.GetBlock(wx, y, wz)).solid;
            const bool airAbove = !BlockRegistry::Get().Def(m_world.GetBlock(wx, y + 1, wz)).solid;
            if (below.solid && !below.liquid && !below.cross && airHere && airAbove) {
                groundY = y;
                break;
            }
        }
        if (groundY < 0) {
            continue; // no valid ground (or chunk not loaded)
        }

        const uint8_t packed = m_world.PackedLightAt({wx, groundY, wz});
        const int skyLight = ChunkLight::Sky(packed);
        const int blockLight = ChunkLight::Block(packed);
        const BlockId belowId = m_world.GetBlock(wx, groundY - 1, wz);

        // Which spawn rule does this spot satisfy? (Data-driven: a mob's
        // MobDef::spawnRule + spawnWeight decides eligibility, not an if-chain.)
        SpawnRule rule;
        if (belowId == blocks::Grass && skyLight >= 9 && !ctx.isNight) {
            if (passive >= kPassiveCap) {
                continue;
            }
            rule = SpawnRule::SurfaceDay;
        } else if (blockLight <= 7 && (ctx.isNight || skyLight <= 7)) {
            if (hostile >= kHostileCap) {
                continue;
            }
            rule = SpawnRule::Dark;
        } else {
            continue;
        }

        // Weighted pick among the mobs whose spawnRule matches (vanilla biome
        // SpawnListEntry weights). With one mob per rule today this is a plain
        // pick; it scales to the full roster without touching this code.
        int totalWeight = 0;
        for (const MobDef& def : kMobDefs) {
            if (def.spawnRule == rule) {
                totalWeight += def.spawnWeight;
            }
        }
        if (totalWeight <= 0) {
            continue;
        }
        int roll = MobRandInt(totalWeight);
        MobType type = MobType::Pig;
        for (int t = 0; t < static_cast<int>(MobType::Count); ++t) {
            const MobDef& def = kMobDefs[t];
            if (def.spawnRule != rule) {
                continue;
            }
            roll -= def.spawnWeight;
            if (roll < 0) {
                type = static_cast<MobType>(t);
                break;
            }
        }

        SpawnMob(type, {static_cast<float>(wx) + 0.5f, static_cast<float>(groundY),
                        static_cast<float>(wz) + 0.5f});
        return; // one spawn per sweep
    }
}

void EntityManager::ShootArrowAt(const glm::vec3& from, const glm::vec3& target) {
    // Vanilla EntitySkeleton.attackEntityWithRangedAttack: aim at the target with
    // an upward arc (+ horizontalDist * 0.2) so the arrow drops onto the player,
    // plus a little inaccuracy.
    const glm::vec3 d = target - from;
    const float horiz = std::sqrt(d.x * d.x + d.z * d.z);
    glm::vec3 dir{d.x, d.y + horiz * 0.2f, d.z};
    const float len = glm::length(dir);
    if (len < 1e-4f) {
        return;
    }
    dir /= len;
    dir += glm::vec3{(MobRand01() - 0.5f) * 0.06f, (MobRand01() - 0.5f) * 0.06f,
                     (MobRand01() - 0.5f) * 0.06f};
    dir = glm::normalize(dir);
    constexpr float kArrowSpeed = 1.6f * 20.0f; // vanilla 1.6 b/tick -> b/s
    SpawnArrow(from, dir * kArrowSpeed, ArrowOwner::Mob, 2.0f, false);
    m_mobSounds.push_back({MobType::Skeleton, from, 4}); // bow shoot
}

} // namespace vc
