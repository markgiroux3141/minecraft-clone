#include "Player.h"

#include <algorithm>
#include <cmath>

#include "vox/platform/Input.h"

#include "world/World.h"

namespace {

constexpr float kGravity = 32.0f;       // blocks/s^2
constexpr float kTerminalSpeed = 60.0f; // max fall speed
constexpr float kJumpSpeed = 9.0f;      // ~1.25 block jump apex
constexpr float kWalkSpeed = 4.3f;
// Simple swimming: in water gravity and sink rate drop, Space swims up,
// and at the surface Space kicks hard enough to climb a 1-block shore.
// The sink terminal is a gentle drift — fast enough to settle, slow
// enough that it can't be mistaken for falling.
constexpr float kWaterGravity = 10.0f;
constexpr float kWaterSinkSpeed = 1.5f;
constexpr float kSwimSpeed = 4.0f;
constexpr float kBreachSpeed = 8.5f;
constexpr float kWaterDrag = 0.55f; // horizontal speed factor in water
constexpr float kLavaDrag = 0.30f;  // M26: lava is molasses — slower still
constexpr float kSprintMultiplier = 1.6f; // LeftControl
constexpr float kFlySpeed = 16.0f;
constexpr float kFlyBoostMultiplier = 4.0f; // LeftControl
constexpr float kLookSensitivity = 0.1f;    // degrees per pixel
constexpr float kSkin = 0.001f;             // gap kept between AABB and geometry
constexpr float kStepHeight = 0.6f;         // M28: auto-climb slabs/stairs (vanilla 0.6)

} // namespace

void Player::Teleport(const glm::vec3& feetPos) {
    m_position = feetPos;
    m_prevPosition = feetPos;
    m_velocity = glm::vec3{0.0f};
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
    if (m_mode == Mode::Fly) {
        m_inWater = false; // noclip: no footsteps/splash
        TickFly(static_cast<float>(dt));
    } else {
        TickWalk(world, static_cast<float>(dt));
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

    const auto liquidAt = [&](float wx, float wy, float wz) -> const vc::BlockDef& {
        return vc::BlockRegistry::Get().Def(world.GetBlock(static_cast<int>(std::floor(wx)),
                                                           static_cast<int>(std::floor(wy)),
                                                           static_cast<int>(std::floor(wz))));
    };
    const vc::BlockDef& bodyDef = liquidAt(m_position.x, m_position.y + 0.4f, m_position.z);
    const bool inWater = bodyDef.liquid;
    const bool inLava = bodyDef.liquidSource == vc::blocks::Lava; // M26: molasses swim
    const bool headInWater = liquidAt(m_position.x, m_position.y + kEyeHeight, m_position.z).liquid;
    m_inWater = inWater; // M22: footstep/splash audio reads this

    float speed = kWalkSpeed;
    if (KeyDown(vox::Key::LeftControl)) {
        speed *= kSprintMultiplier;
    }

    if (inWater) {
        // Swimming steers where you aim (W toward the look direction);
        // Space always swims straight up, with a breach kick at the
        // surface so you can climb ashore. Lava drags harder and the
        // vertical kicks are weaker — you wade through it slowly.
        speed *= inLava ? kLavaDrag : kWaterDrag;
        const float vScale = inLava ? kLavaDrag / kWaterDrag : 1.0f;
        const glm::vec3 wish = SwimWishDir();
        m_velocity.x = wish.x * speed;
        m_velocity.z = wish.z * speed;
        if (KeyDown(vox::Key::Space)) {
            m_velocity.y = (headInWater ? kSwimSpeed : kBreachSpeed) * vScale;
        } else if (wish.y != 0.0f) {
            m_velocity.y = wish.y * speed;
        } else {
            m_velocity.y =
                std::max(m_velocity.y - kWaterGravity * dt, -kWaterSinkSpeed * vScale);
        }
    } else {
        const glm::vec3 wish = HorizontalWishDir();
        m_velocity.x = wish.x * speed;
        m_velocity.z = wish.z * speed;
        m_velocity.y = std::max(m_velocity.y - kGravity * dt, -kTerminalSpeed);
        if (m_grounded && KeyDown(vox::Key::Space)) {
            m_velocity.y = kJumpSpeed;
        }
    }

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
