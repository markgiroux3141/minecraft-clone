#include "vox/renderer/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace vox {

void PerspectiveCamera::SetPerspective(float fovYDegrees, float nearClip, float farClip) {
    m_fovY = fovYDegrees;
    m_near = nearClip;
    m_far = farClip;
    RecalculateProjection();
}

void PerspectiveCamera::SetViewportSize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    m_aspect = static_cast<float>(width) / static_cast<float>(height);
    RecalculateProjection();
}

void PerspectiveCamera::SetRotation(float yawDegrees, float pitchDegrees) {
    m_yaw = yawDegrees;
    m_pitch = pitchDegrees;
}

void PerspectiveCamera::LookAt(const glm::vec3& target) {
    const glm::vec3 direction = glm::normalize(target - m_position);
    m_pitch = glm::degrees(glm::asin(direction.y));
    m_yaw = glm::degrees(glm::atan(direction.z, direction.x));
}

glm::vec3 PerspectiveCamera::Forward() const {
    const float yaw = glm::radians(m_yaw);
    const float pitch = glm::radians(m_pitch);
    return glm::normalize(glm::vec3{
        glm::cos(pitch) * glm::cos(yaw),
        glm::sin(pitch),
        glm::cos(pitch) * glm::sin(yaw),
    });
}

glm::vec3 PerspectiveCamera::Right() const {
    return glm::normalize(glm::cross(Forward(), glm::vec3{0.0f, 1.0f, 0.0f}));
}

glm::mat4 PerspectiveCamera::View() const {
    return glm::lookAt(m_position, m_position + Forward(), glm::vec3{0.0f, 1.0f, 0.0f});
}

void PerspectiveCamera::RecalculateProjection() {
    m_projection = glm::perspective(glm::radians(m_fovY), m_aspect, m_near, m_far);
}

} // namespace vox
