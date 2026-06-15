#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace vox {

// First-person perspective camera. Orientation is yaw/pitch in degrees:
// yaw -90 looks down -Z, pitch is clamped to (-90, 90) by callers.
class PerspectiveCamera {
public:
    void SetPerspective(float fovYDegrees, float nearClip, float farClip);
    void SetViewportSize(uint32_t width, uint32_t height);

    const glm::vec3& Position() const { return m_position; }
    void SetPosition(const glm::vec3& position) { m_position = position; }

    float Yaw() const { return m_yaw; }
    float Pitch() const { return m_pitch; }
    void SetRotation(float yawDegrees, float pitchDegrees);

    // Roll about the view (forward) axis, in degrees. Independent of
    // SetRotation so callers can layer a transient roll on top (the hurt /
    // death camera tilt). 0 = level.
    float Roll() const { return m_roll; }
    void SetRoll(float rollDegrees) { m_roll = rollDegrees; }

    // Point the camera at a world position from its current position.
    void LookAt(const glm::vec3& target);

    glm::vec3 Forward() const;
    glm::vec3 Right() const;

    glm::mat4 View() const;
    const glm::mat4& Projection() const { return m_projection; }
    glm::mat4 ViewProjection() const { return m_projection * View(); }

private:
    void RecalculateProjection();

    glm::vec3 m_position{0.0f};
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_roll = 0.0f;

    float m_fovY = 70.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
    glm::mat4 m_projection{1.0f};
};

} // namespace vox
