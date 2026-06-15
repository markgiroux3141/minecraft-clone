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
    bool Grounded() const { return m_grounded; } // vanilla dig-speed penalty input
    bool InWater() const { return m_inWater; }    // M22: footstep/splash audio
    float Yaw() const { return m_yaw; }
    float Pitch() const { return m_pitch; }

    // M30 vitals (vanilla 1.12 values). Health 0..20 (2 = one heart), food
    // 0..20 (2 = one drumstick), air -20..300 (300 = full breath). Ticked at
    // 20 TPS in Tick() while in Walk mode; Fly is creative — no damage/hunger.
    float Health() const { return m_health; }
    int FoodLevel() const { return m_foodLevel; }
    float Saturation() const { return m_saturation; }
    float Exhaustion() const { return m_exhaustion; }
    int Air() const { return m_air; }
    bool Dead() const { return m_dead; }
    bool OnFire() const { return m_fireTicks > 0; } // drives the fire overlay
    // Restore from a save (values are clamped to valid ranges).
    void SetVitals(float health, int food, float saturation, float exhaustion, int air);
    // Reset to full health/food/air and teleport to a spawn point.
    void Respawn(const glm::vec3& feetPos);
    // Extra hunger drain from actions GameApp drives (block breaking).
    void AddExhaustion(float amount);

    // M30 hurt-camera tilt (vanilla EntityRenderer.hurtCameraEffect): the roll
    // in degrees to layer onto the camera this frame, interpolated by the
    // render alpha. Decays to 0 over the hurt window after each hit.
    float CameraRoll(double alpha) const;
    // True exactly once after a hit lands (drives the hurt sound); clears on read.
    bool ConsumeHurt();

    // Does the player's AABB overlap this block? (placement check)
    bool Intersects(const glm::ivec3& block) const;

    static constexpr float kHalfWidth = 0.3f;
    static constexpr float kHeight = 1.8f;
    static constexpr float kEyeHeight = 1.62f;
    static constexpr float kMaxHealth = 20.0f;
    static constexpr int kMaxAir = 300; // ticks of breath underwater (15 s)

private:
    void TickWalk(const vc::World& world, float dt);
    void TickFly(float dt);
    // M30: environmental damage (lava/fire/cactus/drown/void) + the FoodStats
    // port (exhaustion -> saturation -> food, natural regen, starvation),
    // run once per tick in Walk mode after movement.
    void TickVitals(const vc::World& world);
    void TickFoodStats();
    // Vanilla EntityLivingBase.attackEntityFrom: applies `amount` health
    // points of damage subject to the hurt-resist window (a bigger hit during
    // the window tops up to the new amount). Sets m_dead at <= 0.
    void ApplyDamage(float amount);
    void Heal(float amount);
    // Player AABB (slightly grown horizontally) overlaps a cactus cell.
    bool TouchingCactus(const vc::World& world) const;
    // Input::IsKeyDown gated on this tick's input flag.
    bool KeyDown(vox::Key key) const;
    // Move along one axis and clamp against block collision boxes
    // (axis-separated AABB collision; M28: per-cell boxes so slabs/stairs
    // are partial). Sets m_grounded when landing; returns true if it hit
    // something on this axis (drives auto-step in TickWalk).
    bool MoveAxis(const vc::World& world, int axis, float delta);
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
    bool m_wasGrounded = false; // last tick's grounded state (auto-step footing gate)
    bool m_inWater = false;     // cached in TickWalk for footstep/splash audio
    bool m_inputEnabled = true; // this tick's input flag (see Tick)
    Mode m_mode = Mode::Walk;

    // M30 vitals.
    float m_health = kMaxHealth;
    int m_foodLevel = 20;
    float m_saturation = 5.0f;   // hidden buffer; drains before food does
    float m_exhaustion = 0.0f;   // 0..40; every 4 spends 1 saturation/food
    int m_foodTimer = 0;         // FoodStats regen/starve cadence counter
    int m_air = kMaxAir;         // breath; counts down submerged, 2 dmg at -20
    int m_hurtResist = 0;        // invulnerability ticks after a hit (max 20)
    float m_lastDamage = 0.0f;   // last hit within the resist window (top-up rule)
    int m_fireTicks = 0;         // burning timer; lava refreshes it to 15 s
    float m_fallDistance = 0.0f; // blocks descended since leaving the ground
    bool m_spawnFallGrace = false; // skip fall damage on the first landing after a teleport
    bool m_dead = false;
    int m_hurtTime = 0;          // counts down from kMaxHurtTime; drives the camera tilt
    bool m_hurtThisTick = false; // a hit landed this tick (one-shot for the hurt sound)

    glm::vec2 m_lastMouse{0.0f};
    bool m_hasLastMouse = false;
};
