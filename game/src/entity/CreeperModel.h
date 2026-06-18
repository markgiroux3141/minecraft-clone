#pragma once

#include <glm/glm.hpp>

#include "entity/MobModel.h"
#include "vox/renderer/BoxModel.h"

namespace vc {

// The creeper (M35) on vox::BoxModel — a verbatim port of vanilla ModelCreeper:
// an 8x8x8 head, an UPRIGHT 8x12x4 body (unlike the quadrupeds' horizontal
// body), and four short 4x6x4 legs (64x32 skin). Same IMobModel contract as the
// other mob models so GameApp's mob pass drives it uniformly. The fuse flash is
// the GameApp render path's job (a u_hurt-style white tint), not the model's.
class CreeperModel : public IMobModel {
public:
    CreeperModel();

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
