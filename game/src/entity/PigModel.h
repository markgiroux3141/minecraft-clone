#pragma once

#include <glm/glm.hpp>

#include "vox/renderer/BoxModel.h"

namespace vc {

// The pig, a quadruped built on vox::BoxModel — a port of vanilla
// ModelQuadruped + ModelPig (head with a snout, a horizontal body, four short
// legs; 64x32 skin). Same render contract as HumanoidModel so GameApp's mob
// pass can treat both uniformly: SetRotationAngles then Render(shader, matrix).
class PigModel {
public:
    PigModel();

    bool Ready() const { return m_model.HasSkin(); }

    // limbSwing/limbSwingAmount drive the trot (diagonal leg pairs); age is
    // unused (no idle sway on the pig); head yaw/pitch in DEGREES.
    void SetRotationAngles(float limbSwing, float limbSwingAmount, float age, float headYawDeg,
                           float headPitchDeg);

    void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const {
        m_model.Render(shader, modelToWorld);
    }

private:
    vox::BoxModel m_model;
    int m_head = -1;
    int m_body = -1;
    int m_leg[4] = {-1, -1, -1, -1};
};

} // namespace vc
