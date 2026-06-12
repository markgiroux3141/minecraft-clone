#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "vox/platform/KeyCodes.h"
#include "vox/renderer/Camera.h"

namespace vc {
class World;
}

// First-person player. Walk mode: AABB collision + gravity, simulated at
// the fixed tick rate and interpolated at render time. Fly mode: noclip,
// also tick-simulated. Mouse look is applied per frame for responsiveness
// (orientation doesn't need interpolation, position does).
class Player {
public:
    enum class Mode : uint8_t { Walk, Fly };

    explicit Player(vox::PerspectiveCamera& camera) : m_camera(camera) {}

    void Teleport(const glm::vec3& feetPos);
    void SetLook(float yawDegrees, float pitchDegrees);

    // With input disabled (inventory screen open) movement keys are
    // ignored but physics continue — you keep falling, water keeps
    // pushing you around.
    void Tick(const vc::World& world, double dt, bool input = true);

    // Moves the camera to the interpolated eye; applies mouse look unless
    // disabled (menus). While disabled the delta tracking resets, so the
    // view doesn't jump when look is re-enabled.
    void OnRender(double alpha, bool mouseLook = true);

    void ToggleMode();
    void SetMode(Mode mode);
    Mode GetMode() const { return m_mode; }

    // Persisted into the world save on exit.
    glm::vec3 Position() const { return m_position; } // feet center
    float Yaw() const { return m_yaw; }
    float Pitch() const { return m_pitch; }

    // Does the player's AABB overlap this block? (placement check)
    bool Intersects(const glm::ivec3& block) const;

    static constexpr float kHalfWidth = 0.3f;
    static constexpr float kHeight = 1.8f;
    static constexpr float kEyeHeight = 1.62f;

private:
    void TickWalk(const vc::World& world, float dt);
    void TickFly(float dt);
    // Input::IsKeyDown gated on this tick's input flag.
    bool KeyDown(vox::Key key) const;
    // Move along one axis and clamp against solid blocks (axis-separated
    // AABB collision). Sets m_grounded when landing.
    void MoveAxis(const vc::World& world, int axis, float delta);
    glm::vec3 HorizontalWishDir() const;
    // Like HorizontalWishDir but W/S follow the full look direction
    // (pitch included) — swimming steers where you aim.
    glm::vec3 SwimWishDir() const;

    vox::PerspectiveCamera& m_camera;
    glm::vec3 m_position{0.0f};     // feet center
    glm::vec3 m_prevPosition{0.0f}; // previous tick, for render interpolation
    glm::vec3 m_velocity{0.0f};
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    bool m_grounded = false;
    bool m_inputEnabled = true; // this tick's input flag (see Tick)
    Mode m_mode = Mode::Walk;

    glm::vec2 m_lastMouse{0.0f};
    bool m_hasLastMouse = false;
};
