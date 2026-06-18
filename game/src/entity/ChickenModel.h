#pragma once

#include <glm/glm.hpp>

#include "entity/MobModel.h"
#include "vox/renderer/BoxModel.h"

namespace vc {

// The chicken — a small biped on vox::BoxModel, ported from vanilla
// ModelChicken: head with a bill + wattle (chin), a near-vertical body, two
// legs, and two side wings (64x32 skin). The legs trot like a quadruped; the
// wings do a gentle idle flap. Same IMobModel contract as the other mobs.
class ChickenModel : public IMobModel {
public:
    ChickenModel();

    bool Ready() const override { return m_model.HasSkin(); }

    // limbSwing/limbSwingAmount drive the leg walk; age drives the wing flap;
    // head yaw/pitch in DEGREES (bill + chin follow the head).
    void SetRotationAngles(float limbSwing, float limbSwingAmount, float age, float headYawDeg,
                           float headPitchDeg) override;

    void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const override {
        m_model.Render(shader, modelToWorld);
    }

private:
    vox::BoxModel m_model;
    int m_head = -1;
    int m_bill = -1;
    int m_chin = -1;
    int m_body = -1;
    int m_rightLeg = -1;
    int m_leftLeg = -1;
    int m_rightWing = -1;
    int m_leftWing = -1;
};

} // namespace vc
