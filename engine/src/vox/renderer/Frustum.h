#pragma once

#include <array>

#include <glm/glm.hpp>

namespace vox {

// View frustum as six inward-facing planes, extracted from a view-projection
// matrix (Gribb–Hartmann). Used to skip drawing chunks outside the camera.
class Frustum {
public:
    static Frustum FromViewProjection(const glm::mat4& viewProj) {
        const glm::mat4 t = glm::transpose(viewProj);
        Frustum frustum;
        frustum.m_planes = {
            t[3] + t[0], // left
            t[3] - t[0], // right
            t[3] + t[1], // bottom
            t[3] - t[1], // top
            t[3] + t[2], // near
            t[3] - t[2], // far
        };
        for (auto& plane : frustum.m_planes) {
            plane /= glm::length(glm::vec3(plane));
        }
        return frustum;
    }

    // Conservative AABB test: true if the box is at least partially inside.
    bool IntersectsAABB(const glm::vec3& min, const glm::vec3& max) const {
        for (const auto& plane : m_planes) {
            // Positive vertex: the box corner furthest along the plane normal.
            const glm::vec3 p{
                plane.x > 0.0f ? max.x : min.x,
                plane.y > 0.0f ? max.y : min.y,
                plane.z > 0.0f ? max.z : min.z,
            };
            if (glm::dot(glm::vec3(plane), p) + plane.w < 0.0f) {
                return false;
            }
        }
        return true;
    }

private:
    std::array<glm::vec4, 6> m_planes{};
};

} // namespace vox
