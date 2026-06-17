#include "entity/Player.h"

#include <algorithm>
#include <cmath>

#include "vox/platform/Input.h"

#include "world/World.h"

namespace {

constexpr float kGravity = 32.0f;       // blocks/s^2
constexpr float kTerminalSpeed = 60.0f; // max fall speed
constexpr float kJumpSpeed = 9.0f;      // ~1.25 block jump apex
constexpr float kWalkSpeed = 4.3f;
// Water/lava movement — ported from vanilla EntityLivingBase.travel (1.12).
// Vertical motion ACCUMULATES under a heavy per-tick drag and weak gravity, so
// a swim-up thrust self-limits to a gentle climb and you bob at the surface
// instead of launching out; horizontal steering stays responsive. Vanilla's
// per-tick numbers convert to our b/s, b/s^2 units at 20 TPS — e.g. its
// "motionY -= 0.02" each tick is a 0.4 b/s decrement = 8 b/s^2.
constexpr float kWaterDrag = 0.55f;    // horizontal swim speed factor (water)
constexpr float kLavaDrag = 0.30f;     // M26: lava is molasses — slower still
constexpr float kWaterVertDrag = 0.8f; // vanilla motionY *= 0.8 each tick
constexpr float kLavaVertDrag = 0.5f;  // vanilla lava motionY *= 0.5 (sinks faster)
constexpr float kWaterGravity = 8.0f;  // vanilla motionY -= 0.02/tick
constexpr float kSwimAccel = 16.0f;    // vanilla handleJumpWater motionY += 0.04/tick
constexpr float kShoreHopSpeed = 6.0f; // vanilla shore pop (motionY = 0.3) to climb out
constexpr float kSprintMultiplier = 1.6f; // LeftControl
constexpr float kFlySpeed = 16.0f;
constexpr float kFlyBoostMultiplier = 4.0f; // LeftControl
constexpr float kLookSensitivity = 0.1f;    // degrees per pixel
constexpr float kSkin = 0.001f;             // gap kept between AABB and geometry
constexpr float kStepHeight = 0.6f;         // M28: auto-climb slabs/stairs (vanilla 0.6)
constexpr int kMaxHurtResist = 20;          // vanilla maxHurtResistantTime (1 s)
constexpr int kMaxHurtTime = 10;            // vanilla maxHurtTime (hurt-tilt window)
constexpr float kPi = 3.14159265358979f;
constexpr float kKnockback = 7.0f;     // M32: horizontal shove speed from a mob hit (b/s)
constexpr float kKnockbackUp = 7.0f;   // and the upward pop
constexpr float kKnockbackDecay = 0.55f; // per-tick falloff (additive over the wish move)

} // namespace

void Player::Teleport(const glm::vec3& feetPos) {
    m_position = feetPos;
    m_prevPosition = feetPos;
    m_velocity = glm::vec3{0.0f};
    // The drop from a teleport (spawn is above the surface) must not deal fall
    // damage — exempt the first landing afterwards (cleared in TickWalk).
    m_fallDistance = 0.0f;
    m_spawnFallGrace = true;
}

void Player::SetLook(float yawDegrees, float pitchDegrees) {
    m_yaw = yawDegrees;
    m_pitch = std::clamp(pitchDegrees, -89.0f, 89.0f);
}

void Player::ToggleMode() {
    SetMode(m_mode == Mode::Walk ? Mode::Fly : Mode::Walk);
}

void Player::SetMode(Mode mode) {
    m_mode = mode;
    m_velocity = glm::vec3{0.0f};
}

glm::vec3 Player::HorizontalWishDir() const {
    const float yawRad = glm::radians(m_yaw);
    const glm::vec3 forward{std::cos(yawRad), 0.0f, std::sin(yawRad)};
    const glm::vec3 right{-forward.z, 0.0f, forward.x};

    glm::vec3 wish{0.0f};
    if (KeyDown(vox::Key::W)) {
        wish += forward;
    }
    if (KeyDown(vox::Key::S)) {
        wish -= forward;
    }
    if (KeyDown(vox::Key::D)) {
        wish += right;
    }
    if (KeyDown(vox::Key::A)) {
        wish -= right;
    }
    return wish == glm::vec3{0.0f} ? wish : glm::normalize(wish);
}

glm::vec3 Player::SwimWishDir() const {
    const float yawRad = glm::radians(m_yaw);
    const float pitchRad = glm::radians(m_pitch);
    const glm::vec3 forward{std::cos(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                            std::sin(yawRad) * std::cos(pitchRad)};
    const glm::vec3 right{-std::sin(yawRad), 0.0f, std::cos(yawRad)};

    glm::vec3 wish{0.0f};
    if (KeyDown(vox::Key::W)) {
        wish += forward;
    }
    if (KeyDown(vox::Key::S)) {
        wish -= forward;
    }
    if (KeyDown(vox::Key::D)) {
        wish += right;
    }
    if (KeyDown(vox::Key::A)) {
        wish -= right;
    }
    return wish == glm::vec3{0.0f} ? wish : glm::normalize(wish);
}

bool Player::KeyDown(vox::Key key) const {
    return m_inputEnabled && vox::Input::IsKeyDown(key);
}

void Player::Tick(const vc::World& world, double dt, bool input) {
    m_prevPosition = m_position;
    m_inputEnabled = input;
    if (m_hurtTime > 0) {
        --m_hurtTime; // hurt-tilt window decays one tick at a time (both modes)
    }
    if (m_mode == Mode::Fly) {
        m_inWater = false; // noclip: no footsteps/splash
        m_fallDistance = 0.0f;
        m_air = kMaxAir; // creative-style: no drowning, no hunger, no damage
        if (m_hurtResist > 0) {
            --m_hurtResist;
        }
        TickFly(static_cast<float>(dt));
    } else {
        TickWalk(world, static_cast<float>(dt));
        TickVitals(world);
    }
    // Remember this tick's footing so next tick's auto-step can require
    // CONTINUOUS ground contact (see TickWalk) — distinguishes walking up
    // stairs from a fall/jump that lands against them.
    m_wasGrounded = m_grounded;
}

void Player::TickWalk(const vc::World& world, float dt) {
    // Freeze until the chunk we're standing in has block data, so the
    // player doesn't fall through still-generating terrain.
    const glm::ivec3 feetChunk{
        static_cast<int>(std::floor(m_position.x)) >> 4,
        std::clamp(static_cast<int>(std::floor(m_position.y)) >> 4, 0,
                   vc::World::kHeightChunks - 1),
        static_cast<int>(std::floor(m_position.z)) >> 4,
    };
    if (!world.GetChunk(feetChunk)) {
        return;
    }

    const float startY = m_position.y; // for fall-distance accumulation

    const auto liquidAt = [&](float wx, float wy, float wz) -> const vc::BlockDef& {
        return vc::BlockRegistry::Get().Def(world.GetBlock(static_cast<int>(std::floor(wx)),
                                                           static_cast<int>(std::floor(wy)),
                                                           static_cast<int>(std::floor(wz))));
    };
    const vc::BlockDef& bodyDef = liquidAt(m_position.x, m_position.y + 0.4f, m_position.z);
    const bool inWater = bodyDef.liquid;
    const bool inLava = bodyDef.liquidSource == vc::blocks::Lava; // M26: molasses swim
    m_inWater = inWater; // M22: footstep/splash audio reads this

    float speed = kWalkSpeed;
    if (KeyDown(vox::Key::LeftControl)) {
        speed *= kSprintMultiplier;
    }

    glm::vec3 swimWish{0.0f}; // set in water; reused by the shore-hop below
    if (inWater) {
        // Vanilla water movement: horizontal steers where you aim; vertical is
        // force-based. A small per-tick thrust (Space, or aiming a move key
        // up/down) fights weak gravity under a heavy 0.8/tick drag, so it
        // self-limits to a gentle ~2 b/s climb and bobs at the surface instead
        // of launching out. Lava drags harder still — you barely rise in it.
        speed *= inLava ? kLavaDrag : kWaterDrag;
        swimWish = SwimWishDir();
        m_velocity.x = swimWish.x * speed;
        m_velocity.z = swimWish.z * speed;

        m_velocity.y *= inLava ? kLavaVertDrag : kWaterVertDrag; // drag momentum
        m_velocity.y -= kWaterGravity * dt;                      // weak buoyant gravity
        if (KeyDown(vox::Key::Space)) {
            m_velocity.y += kSwimAccel * dt;                     // swim straight up
        } else if (swimWish.y != 0.0f) {
            m_velocity.y += swimWish.y * kSwimAccel * dt;        // aim up/down to rise/dive
        }
    } else {
        const glm::vec3 wish = HorizontalWishDir();
        m_velocity.x = wish.x * speed;
        m_velocity.z = wish.z * speed;
        m_velocity.y = std::max(m_velocity.y - kGravity * dt, -kTerminalSpeed);
        if (m_grounded && KeyDown(vox::Key::Space)) {
            m_velocity.y = kJumpSpeed;
            AddExhaustion(KeyDown(vox::Key::LeftControl) ? 0.2f : 0.05f); // vanilla jump cost
        }
    }

    // M32 knockback: a mob hit adds a decaying horizontal impulse ON TOP of the
    // wish velocity (which is re-set from input each tick), so the shove still
    // reads through movement for a few ticks before fading.
    m_velocity.x += m_knockback.x;
    m_velocity.z += m_knockback.z;
    m_knockback *= kKnockbackDecay;

    m_grounded = false;
    MoveAxis(world, 1, m_velocity.y * dt);
    const bool groundedBeforeStep = m_grounded;

    // Horizontal move. If it's blocked while grounded (and not swimming), try
    // the same move lifted by kStepHeight and settled back down — this is what
    // lets you walk up slabs/stairs (and any <=0.6 lip) without jumping.
    const glm::vec3 flatStart = m_position;
    const glm::vec3 flatVel = m_velocity;
    const bool hitX = MoveAxis(world, 0, m_velocity.x * dt);
    const bool hitZ = MoveAxis(world, 2, m_velocity.z * dt);

    // Auto-step the lip (slab/stair, <= kStepHeight) only with CONTINUOUS
    // footing — grounded both this tick AND last tick. Requiring last tick too
    // is what stops a jump or fall that merely brushes a staircase from being
    // yanked up onto it: on the landing frame you're grounded-this-tick but
    // were airborne last tick, so no step fires. Walking up stairs keeps you
    // continuously grounded, so it still works.
    if ((hitX || hitZ) && groundedBeforeStep && m_wasGrounded && !inWater) {
        const glm::vec3 flatEnd = m_position;
        const glm::vec3 flatEndVel = m_velocity;
        m_position = flatStart;
        m_velocity = flatVel;
        const float beforeLift = m_position.y;
        MoveAxis(world, 1, kStepHeight);          // lift over the lip (clamped by ceilings)
        const float lifted = m_position.y - beforeLift; // how far we actually rose
        MoveAxis(world, 0, flatVel.x * dt);       // re-attempt the move up there
        MoveAxis(world, 2, flatVel.z * dt);
        MoveAxis(world, 1, -lifted);              // settle back down onto the step (or to start)

        const float stepGain = (m_position.x - flatStart.x) * (m_position.x - flatStart.x) +
                               (m_position.z - flatStart.z) * (m_position.z - flatStart.z);
        const float flatGain = (flatEnd.x - flatStart.x) * (flatEnd.x - flatStart.x) +
                               (flatEnd.z - flatStart.z) * (flatEnd.z - flatStart.z);
        if (stepGain <= flatGain + 1e-6f) {
            // Stepping didn't advance us (a full wall, not a lip) — keep the
            // plain move exactly as it landed.
            m_position = flatEnd;
            m_velocity = flatEndVel;
            m_grounded = true;
        } else {
            // Took the step; keep horizontal momentum for the next tick.
            m_velocity.x = flatVel.x;
            m_velocity.z = flatVel.z;
        }
    }

    // Vanilla shore-hop (EntityLivingBase.travel): swimming INTO a wall pops you
    // up just enough to climb a 1-block lip onto land. This is the only way out
    // of water under your own power — open water just bobs you at the surface.
    if (inWater && (hitX || hitZ) && (swimWish.x != 0.0f || swimWish.z != 0.0f)) {
        m_velocity.y = kShoreHopSpeed;
    }

    // Fall damage (vanilla updateFallState): accumulate descent while airborne;
    // on landing, ceil(distance - 3) health points. Water cancels the fall, and
    // the first landing after a teleport (spawn drop) is exempt.
    if (inWater) {
        m_fallDistance = 0.0f;
    } else if (m_grounded) {
        if (!m_spawnFallGrace && m_fallDistance > 3.0f) {
            ApplyDamage(std::ceil(m_fallDistance - 3.0f), /*bypassArmor=*/true);
        }
        m_fallDistance = 0.0f;
        m_spawnFallGrace = false;
    } else if (m_position.y < startY) {
        m_fallDistance += startY - m_position.y;
    }
}

void Player::TickFly(float dt) {
    glm::vec3 velocity = HorizontalWishDir();
    if (KeyDown(vox::Key::Space)) {
        velocity.y += 1.0f;
    }
    if (KeyDown(vox::Key::LeftShift)) {
        velocity.y -= 1.0f;
    }
    float speed = kFlySpeed;
    if (KeyDown(vox::Key::LeftControl)) {
        speed *= kFlyBoostMultiplier;
    }
    m_position += velocity * speed * dt;
    m_grounded = false;
}

bool Player::MoveAxis(const vc::World& world, int axis, float delta) {
    if (delta == 0.0f) {
        return false;
    }
    m_position[axis] += delta;

    const glm::vec3 boxMin = m_position - glm::vec3{kHalfWidth, 0.0f, kHalfWidth};
    const glm::vec3 boxMax = m_position + glm::vec3{kHalfWidth, kHeight, kHalfWidth};
    const glm::ivec3 lo{static_cast<int>(std::floor(boxMin.x + 1e-5f)),
                        static_cast<int>(std::floor(boxMin.y + 1e-5f)),
                        static_cast<int>(std::floor(boxMin.z + 1e-5f))};
    const glm::ivec3 hi{static_cast<int>(std::floor(boxMax.x - 1e-5f)),
                        static_cast<int>(std::floor(boxMax.y - 1e-5f)),
                        static_cast<int>(std::floor(boxMax.z - 1e-5f))};

    // The two axes perpendicular to the move; a block box only resolves the
    // move if the player overlaps it on BOTH of them (M28: with partial slab/
    // stair boxes a cell can be occupied yet not in the player's path).
    const int b = (axis + 1) % 3;
    const int c = (axis + 2) % 3;

    // Player extents toward each side of the move axis, and the LEADING /
    // trailing faces BEFORE this move. A box only clamps the move if it lies
    // ahead of the leading face (vanilla AxisAlignedBB.calculateOffset
    // semantics): a box the player already straddles on this axis must not
    // resolve, or moving AWAY from a partial box you overlap (e.g. the tall
    // quarter of a stair beside your feet — the player is wider than the
    // half-cell tread) would shove you backward across it.
    const float extentPos = (axis == 1) ? kHeight : kHalfWidth;
    const float extentNeg = (axis == 1) ? 0.0f : kHalfWidth;
    const float preLead = (m_position[axis] - delta) + (delta > 0.0f ? extentPos : -extentNeg);

    bool collided = false;
    float resolved = m_position[axis];
    for (int by = lo.y; by <= hi.y; ++by) {
        for (int bz = lo.z; bz <= hi.z; ++bz) {
            for (int bx = lo.x; bx <= hi.x; ++bx) {
                vc::World::BlockBox boxes[2];
                const int n = world.CollisionBoxesAt(bx, by, bz, boxes);
                const glm::vec3 cell{static_cast<float>(bx), static_cast<float>(by),
                                     static_cast<float>(bz)};
                for (int i = 0; i < n; ++i) {
                    const glm::vec3 bMin = cell + boxes[i].min;
                    const glm::vec3 bMax = cell + boxes[i].max;
                    // Perpendicular-axis overlap with the player AABB.
                    if (boxMin[b] >= bMax[b] || boxMax[b] <= bMin[b] || boxMin[c] >= bMax[c] ||
                        boxMax[c] <= bMin[c]) {
                        continue;
                    }
                    if (delta > 0.0f) {
                        if (bMin[axis] < preLead - kSkin) {
                            continue; // box behind / straddled — not in our path
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
        m_position[axis] = resolved;
        m_velocity[axis] = 0.0f;
        if (axis == 1 && delta < 0.0f) {
            m_grounded = true;
        }
    }
    return collided;
}

void Player::OnRender(double alpha, bool mouseLook) {
    if (mouseLook) {
        const glm::vec2 mouse = vox::Input::MousePosition();
        if (m_hasLastMouse) {
            const glm::vec2 delta = mouse - m_lastMouse;
            m_yaw += delta.x * kLookSensitivity;
            m_pitch = std::clamp(m_pitch - delta.y * kLookSensitivity, -89.0f, 89.0f);
        }
        m_lastMouse = mouse;
        m_hasLastMouse = true;
    } else {
        m_hasLastMouse = false;
    }

    const glm::vec3 feet = glm::mix(m_prevPosition, m_position, static_cast<float>(alpha));
    m_camera.SetPosition(feet + glm::vec3{0.0f, kEyeHeight, 0.0f});
    m_camera.SetRotation(m_yaw, m_pitch);
}

bool Player::Intersects(const glm::ivec3& block) const {
    const glm::vec3 boxMin = m_position - glm::vec3{kHalfWidth, 0.0f, kHalfWidth};
    const glm::vec3 boxMax = m_position + glm::vec3{kHalfWidth, kHeight, kHalfWidth};
    const glm::vec3 cellMin{block};
    const glm::vec3 cellMax = cellMin + 1.0f;
    return boxMin.x < cellMax.x && boxMax.x > cellMin.x && boxMin.y < cellMax.y &&
           boxMax.y > cellMin.y && boxMin.z < cellMax.z && boxMax.z > cellMin.z;
}

// --- M30 vitals --------------------------------------------------------------

void Player::AddExhaustion(float amount) {
    m_exhaustion = std::min(m_exhaustion + amount, 40.0f);
}

void Player::Heal(float amount) {
    if (!m_dead && m_health > 0.0f) {
        m_health = std::min(m_health + amount, kMaxHealth);
    }
}

float Player::AbsorbArmor(float amount, bool bypassArmor) {
    if (bypassArmor || m_armorDefense <= 0.0f) {
        return amount;
    }
    // Book the raw amount for durability wear (vanilla wears armor on the
    // incoming damage, before reduction), then apply CombatRules.
    m_armorWear += amount;
    const float f = 2.0f + m_armorToughness / 4.0f;
    const float clamped =
        std::clamp(m_armorDefense - amount / f, m_armorDefense * 0.2f, 20.0f);
    return amount * (1.0f - clamped / 25.0f);
}

bool Player::ApplyDamage(float amount, bool bypassArmor) {
    if (m_dead || amount <= 0.0f) {
        return false;
    }
    bool freshHit = false;
    // Vanilla EntityLivingBase.attackEntityFrom hurt-resist window: within the
    // first half of the window a fresh hit only lands the amount ABOVE the last
    // one; otherwise it lands in full and arms the window. m_lastDamage tracks
    // the raw (pre-armor) amount, like vanilla; armor reduces only the health
    // delta actually subtracted.
    if (m_hurtResist > kMaxHurtResist / 2) {
        if (amount <= m_lastDamage) {
            return false;
        }
        m_health -= AbsorbArmor(amount - m_lastDamage, bypassArmor);
        m_lastDamage = amount;
    } else {
        m_lastDamage = amount;
        m_hurtResist = kMaxHurtResist;
        m_health -= AbsorbArmor(amount, bypassArmor);
        // Fresh hit: arm the hurt-tilt window and the one-shot hurt sound
        // (vanilla only does this on a full hit, not a within-window top-up).
        m_hurtTime = kMaxHurtTime;
        m_hurtThisTick = true;
        m_hurtRollSign = 1.0f; // undirected by default (environmental damage)
        freshHit = true;
    }
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
    }
    return freshHit;
}

void Player::SetArmorStats(float defensePoints, float toughness) {
    m_armorDefense = defensePoints;
    m_armorToughness = toughness;
}

float Player::ConsumeArmorWear() {
    const float wear = m_armorWear;
    m_armorWear = 0.0f;
    return wear;
}

void Player::Hurt(float amount, const glm::vec3& fromPos) {
    if (!ApplyDamage(amount)) {
        return; // dead, no-damage, or absorbed by the resist window — no knockback
    }
    // Knockback away from the source (vanilla EntityLivingBase.knockBack);
    // horizontal direction from the attacker to us, with a small upward pop.
    glm::vec3 away = m_position - fromPos;
    away.y = 0.0f;
    if (glm::length(away) > 1e-4f) {
        away = glm::normalize(away);
    } else {
        away = {0.0f, 0.0f, 1.0f};
    }
    m_knockback = away * kKnockback;
    if (m_grounded) {
        m_velocity.y = kKnockbackUp;
    }
    // Directional hurt tilt: lean by which side the hit came from (sign of the
    // shove projected onto the look-right axis).
    const float yawRad = glm::radians(m_yaw);
    const glm::vec3 right{-std::sin(yawRad), 0.0f, std::cos(yawRad)};
    m_hurtRollSign = glm::dot(away, right) >= 0.0f ? 1.0f : -1.0f;
}

void Player::ExternalPush(const vc::World& world, float dx, float dz) {
    if (m_mode == Mode::Fly) {
        return; // creative noclip: mobs don't shove you
    }
    if (dx != 0.0f) {
        MoveAxis(world, 0, dx);
    }
    if (dz != 0.0f) {
        MoveAxis(world, 2, dz);
    }
}

void Player::SetVitals(float health, int food, float saturation, float exhaustion, int air) {
    m_health = std::clamp(health, 0.0f, kMaxHealth);
    m_foodLevel = std::clamp(food, 0, 20);
    m_saturation = std::clamp(saturation, 0.0f, 20.0f);
    m_exhaustion = std::clamp(exhaustion, 0.0f, 40.0f);
    m_air = std::clamp(air, -20, kMaxAir);
    m_dead = m_health <= 0.0f;
}

void Player::Respawn(const glm::vec3& feetPos) {
    m_health = kMaxHealth;
    m_foodLevel = 20;
    m_saturation = 5.0f;
    m_exhaustion = 0.0f;
    m_foodTimer = 0;
    m_air = kMaxAir;
    m_fireTicks = 0;
    m_hurtResist = 0;
    m_lastDamage = 0.0f;
    m_hurtTime = 0;
    m_hurtThisTick = false;
    m_hurtRollSign = 1.0f;
    m_knockback = glm::vec3{0.0f};
    m_dead = false;
    m_mode = Mode::Walk;
    Teleport(feetPos); // resets velocity + arms spawn fall-grace
}

float Player::CameraRoll(double alpha) const {
    // Vanilla EntityRenderer.hurtCameraEffect: f = (hurtTime - partialTicks) /
    // maxHurtTime, tilt = sin(f^4 * pi) * 14 degrees — a sharp spike that eases
    // back to level over the window. attackedAtYaw is 0 for environmental
    // damage, so this is a pure (undirected) roll.
    float f = static_cast<float>(m_hurtTime) - static_cast<float>(alpha);
    if (f <= 0.0f) {
        return 0.0f;
    }
    f /= static_cast<float>(kMaxHurtTime);
    f = std::sin(f * f * f * f * kPi);
    return f * 14.0f * m_hurtRollSign;
}

bool Player::ConsumeHurt() {
    const bool hurt = m_hurtThisTick;
    m_hurtThisTick = false;
    return hurt;
}

bool Player::TouchingCactus(const vc::World& world) const {
    // Our cactus is a full-cube collision (the vanilla 14/16 inset is backlog),
    // so the player never actually overlaps the cell — grow the test box
    // horizontally by 0.1 so pressing up against a cactus still hurts, while
    // keeping the vertical range inset so standing on top is safe.
    constexpr float kGrow = 0.1f;
    const glm::vec3 lo{m_position.x - kHalfWidth - kGrow, m_position.y + kGrow,
                       m_position.z - kHalfWidth - kGrow};
    const glm::vec3 hi{m_position.x + kHalfWidth + kGrow, m_position.y + kHeight - kGrow,
                       m_position.z + kHalfWidth + kGrow};
    for (int by = static_cast<int>(std::floor(lo.y)); by <= static_cast<int>(std::floor(hi.y));
         ++by) {
        for (int bz = static_cast<int>(std::floor(lo.z)); bz <= static_cast<int>(std::floor(hi.z));
             ++bz) {
            for (int bx = static_cast<int>(std::floor(lo.x));
                 bx <= static_cast<int>(std::floor(hi.x)); ++bx) {
                if (world.GetBlock(bx, by, bz) == vc::blocks::Cactus) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Player::TickVitals(const vc::World& world) {
    if (m_hurtResist > 0) {
        --m_hurtResist;
    }
    if (m_dead) {
        return;
    }

    const auto def = [&](float x, float y, float z) -> const vc::BlockDef& {
        return vc::BlockRegistry::Get().Def(world.GetBlock(static_cast<int>(std::floor(x)),
                                                           static_cast<int>(std::floor(y)),
                                                           static_cast<int>(std::floor(z))));
    };
    const vc::BlockDef& body = def(m_position.x, m_position.y + 0.4f, m_position.z);
    const vc::BlockDef& head = def(m_position.x, m_position.y + kEyeHeight, m_position.z);
    const bool inLava = body.liquidSource == vc::blocks::Lava;
    const bool inWater = body.liquidSource == vc::blocks::Water;
    const bool headInWater = head.liquidSource == vc::blocks::Water;

    // Lava: 4 dmg/tick, and it keeps you burning for 15 s after you climb out.
    if (inLava) {
        ApplyDamage(4.0f);
        m_fireTicks = 15 * 20;
    }
    // Water douses fire instantly (vanilla extinguish) — dive in to escape it.
    if (inWater) {
        m_fireTicks = 0;
    }
    // Burning: 1 dmg every second the fire timer runs. Vanilla's ON_FIRE
    // (burn-over-time) bypasses armor — unlike standing in lava.
    if (m_fireTicks > 0) {
        --m_fireTicks;
        if (m_fireTicks % 20 == 0) {
            ApplyDamage(1.0f, /*bypassArmor=*/true);
        }
    }
    // Cactus: 1 dmg/tick while the (grown) AABB touches one.
    if (TouchingCactus(world)) {
        ApplyDamage(1.0f);
    }
    // Drowning: 15 s of breath, then 2 dmg/sec (vanilla 300 -> -20 air cycle).
    if (headInWater) {
        --m_air;
        if (m_air <= -20) {
            m_air = 0;
            ApplyDamage(2.0f, /*bypassArmor=*/true);
        }
    } else {
        m_air = kMaxAir;
    }
    // The void.
    if (m_position.y < -64.0f) {
        ApplyDamage(4.0f, /*bypassArmor=*/true);
    }

    // Movement exhaustion (vanilla addMovementStat): sprinting drains far
    // faster than walking; both feed the FoodStats drain below.
    const float dx = m_position.x - m_prevPosition.x;
    const float dz = m_position.z - m_prevPosition.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist > 0.001f) {
        const bool sprint = m_grounded && KeyDown(vox::Key::LeftControl);
        AddExhaustion((sprint ? 0.1f : 0.01f) * dist);
    }

    TickFoodStats();
}

void Player::TickFoodStats() {
    // Port of FoodStats.onUpdate (naturalRegeneration on, normal difficulty).
    if (m_exhaustion > 4.0f) {
        m_exhaustion -= 4.0f;
        if (m_saturation > 0.0f) {
            m_saturation = std::max(m_saturation - 1.0f, 0.0f);
        } else {
            m_foodLevel = std::max(m_foodLevel - 1, 0);
        }
    }

    const bool shouldHeal = m_health > 0.0f && m_health < kMaxHealth;
    if (m_saturation > 0.0f && shouldHeal && m_foodLevel >= 20) {
        // Fast regen while saturated and full: heals quickly, costs exhaustion.
        if (++m_foodTimer >= 10) {
            const float f = std::min(m_saturation, 6.0f);
            Heal(f / 6.0f);
            AddExhaustion(f);
            m_foodTimer = 0;
        }
    } else if (m_foodLevel >= 18 && shouldHeal) {
        // Slow regen on a full-ish belly.
        if (++m_foodTimer >= 80) {
            Heal(1.0f);
            AddExhaustion(6.0f);
            m_foodTimer = 0;
        }
    } else if (m_foodLevel <= 0) {
        // Starvation (normal difficulty: floors at 1 health, never kills).
        if (++m_foodTimer >= 80) {
            if (m_health > 1.0f) {
                ApplyDamage(1.0f, /*bypassArmor=*/true);
            }
            m_foodTimer = 0;
        }
    } else {
        m_foodTimer = 0;
    }
}
