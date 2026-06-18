#pragma once

#include <glm/glm.hpp>

#include "entity/MobModel.h"
#include "vox/renderer/BoxModel.h"

namespace vc {

// The cow, a quadruped on vox::BoxModel — a port of vanilla ModelCow (extends
// ModelQuadruped(12, 0)): a head with two horns + a snout-less face, a deep
// horizontal body with a small udder, and four 12px legs (64x32 skin). Same
// IMobModel contract as PigModel so GameApp's mob pass treats them uniformly.
class CowModel : public IMobModel {
public:
    CowModel();

    bool Ready() const override { return m_model.HasSkin(); }

    // limbSwing/limbSwingAmount drive the trot (diagonal leg pairs); age unused;
    // head yaw/pitch in DEGREES.
    void SetRotationAngles(float limbSwing, float limbSwingAmount, float age, float headYawDeg,
                           float headPitchDeg) override;

    void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const override {
        m_model.Render(shader, modelToWorld);
    }

private:
    vox::BoxModel m_model;
    int m_head = -1;
    int m_body = -1;
    int m_leg[4] = {-1, -1, -1, -1};
};

} // namespace vc
