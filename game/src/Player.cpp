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
constexpr float kSprintMultiplier = 1.6f; // LeftControl
constexpr float kFlySpeed = 16.0f;
constexpr float kFlyBoostMultiplier = 4.0f; // LeftControl
constexpr float kLookSensitivity = 0.1f;    // degrees per pixel
constexpr float kSkin = 0.001f;             // gap kept between AABB and geometry

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

    const auto waterAt = [&](float wx, float wy, float wz) {
        return vc::BlockRegistry::Get()
            .Def(world.GetBlock(static_cast<int>(std::floor(wx)),
                                static_cast<int>(std::floor(wy)),
                                static_cast<int>(std::floor(wz))))
            .liquid;
    };
    const bool inWater = waterAt(m_position.x, m_position.y + 0.4f, m_position.z);
    const bool headInWater = waterAt(m_position.x, m_position.y + kEyeHeight, m_position.z);
    m_inWater = inWater; // M22: footstep/splash audio reads this

    float speed = kWalkSpeed;
    if (KeyDown(vox::Key::LeftControl)) {
        speed *= kSprintMultiplier;
    }

    if (inWater) {
        // Swimming steers where you aim (W toward the look direction);
        // Space always swims straight up, with a breach kick at the
        // surface so you can climb ashore.
        speed *= kWaterDrag;
        const glm::vec3 wish = SwimWishDir();
        m_velocity.x = wish.x * speed;
        m_velocity.z = wish.z * speed;
        if (KeyDown(vox::Key::Space)) {
            m_velocity.y = headInWater ? kSwimSpeed : kBreachSpeed;
        } else if (wish.y != 0.0f) {
            m_velocity.y = wish.y * speed;
        } else {
            m_velocity.y = std::max(m_velocity.y - kWaterGravity * dt, -kWaterSinkSpeed);
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
    MoveAxis(world, 0, m_velocity.x * dt);
    MoveAxis(world, 2, m_velocity.z * dt);
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

void Player::MoveAxis(const vc::World& world, int axis, float delta) {
    if (delta == 0.0f) {
        return;
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

    bool collided = false;
    float resolved = m_position[axis];
    for (int by = lo.y; by <= hi.y; ++by) {
        for (int bz = lo.z; bz <= hi.z; ++bz) {
            for (int bx = lo.x; bx <= hi.x; ++bx) {
                if (!world.IsSolid(bx, by, bz)) {
                    continue;
                }
                collided = true;
                const int cell = (axis == 0) ? bx : (axis == 1) ? by : bz;
                if (delta > 0.0f) {
                    // Push back so the AABB's leading face sits on the cell.
                    const float extent = (axis == 1) ? kHeight : kHalfWidth;
                    resolved = std::min(resolved, static_cast<float>(cell) - extent - kSkin);
                } else {
                    const float extent = (axis == 1) ? 0.0f : kHalfWidth;
                    resolved =
                        std::max(resolved, static_cast<float>(cell + 1) + extent + kSkin);
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
