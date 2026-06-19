#pragma once

#include <glm/glm.hpp>

#include "entity/MobModel.h"
#include "vox/renderer/BoxModel.h"

namespace vc {

// The spider, an eight-legged crawler on vox::BoxModel — a port of vanilla
// ModelSpider: a forward head, a round neck, a deep body, and eight 16px legs
// that scuttle in four opposed phase pairs (64x32 skin). Same IMobModel contract
// as the other mob models so GameApp's mob pass treats it uniformly. The head
// yaw/pitch are accepted but the in-world pass passes 0 (no head tracking yet).
class SpiderModel : public IMobModel {
public:
    SpiderModel();

    bool Ready() const override { return m_model.HasSkin(); }

    // limbSwing/limbSwingAmount drive the eight-leg scuttle; age unused; head
    // yaw/pitch in DEGREES (0 from the in-world pass).
    void SetRotationAngles(float limbSwing, float limbSwingAmount, float age, float headYawDeg,
                           float headPitchDeg) override;

    void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const override {
        m_model.Render(shader, modelToWorld);
    }

private:
    vox::BoxModel m_model;
    int m_head = -1;
    int m_neck = -1;
    int m_body = -1;
    int m_leg[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
};

} // namespace vc
