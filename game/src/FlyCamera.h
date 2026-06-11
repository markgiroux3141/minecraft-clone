#pragma once

#include <glm/glm.hpp>

#include "vox/renderer/Camera.h"

// Debug fly camera: hold right mouse button to look around, WASD to move,
// Space/LeftShift for up/down, LeftControl for a speed boost.
class FlyCamera {
public:
    explicit FlyCamera(vox::PerspectiveCamera& camera) : m_camera(camera) {}

    // Per-frame (not per-tick): camera movement should be as smooth as the framerate.
    void OnUpdate(double dt);

private:
    vox::PerspectiveCamera& m_camera;
    glm::vec2 m_lastMouse{0.0f};
    bool m_looking = false;

    float m_moveSpeed = 8.0f;       // units per second
    float m_boostMultiplier = 4.0f; // while LeftControl is held
    float m_lookSensitivity = 0.1f; // degrees per pixel
};
