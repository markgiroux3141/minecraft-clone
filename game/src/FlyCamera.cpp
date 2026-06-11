#include "FlyCamera.h"

#include <algorithm>

#include "vox/core/Application.h"
#include "vox/platform/Input.h"

void FlyCamera::OnUpdate(double dt) {
    auto& window = vox::Application::Get().GetWindow();

    // Mouselook while the right mouse button is held.
    if (vox::Input::IsMouseButtonDown(vox::MouseButton::Right)) {
        const glm::vec2 mouse = vox::Input::MousePosition();
        if (!m_looking) {
            m_looking = true;
            window.SetCursorCaptured(true);
        } else {
            const glm::vec2 delta = mouse - m_lastMouse;
            const float yaw = m_camera.Yaw() + delta.x * m_lookSensitivity;
            const float pitch =
                std::clamp(m_camera.Pitch() - delta.y * m_lookSensitivity, -89.0f, 89.0f);
            m_camera.SetRotation(yaw, pitch);
        }
        m_lastMouse = mouse;
    } else if (m_looking) {
        m_looking = false;
        window.SetCursorCaptured(false);
    }

    glm::vec3 move{0.0f};
    const glm::vec3 forward = m_camera.Forward();
    const glm::vec3 right = m_camera.Right();
    constexpr glm::vec3 up{0.0f, 1.0f, 0.0f};

    if (vox::Input::IsKeyDown(vox::Key::W)) {
        move += forward;
    }
    if (vox::Input::IsKeyDown(vox::Key::S)) {
        move -= forward;
    }
    if (vox::Input::IsKeyDown(vox::Key::D)) {
        move += right;
    }
    if (vox::Input::IsKeyDown(vox::Key::A)) {
        move -= right;
    }
    if (vox::Input::IsKeyDown(vox::Key::Space)) {
        move += up;
    }
    if (vox::Input::IsKeyDown(vox::Key::LeftShift)) {
        move -= up;
    }

    if (move != glm::vec3{0.0f}) {
        float speed = m_moveSpeed;
        if (vox::Input::IsKeyDown(vox::Key::LeftControl)) {
            speed *= m_boostMultiplier;
        }
        m_camera.SetPosition(m_camera.Position() +
                             glm::normalize(move) * speed * static_cast<float>(dt));
    }
}
