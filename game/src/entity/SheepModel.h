#pragma once

#include <glm/glm.hpp>

#include "entity/MobModel.h"
#include "vox/renderer/BoxModel.h"

namespace vc {

// The sheep — two stacked quadruped models, mirroring vanilla's RenderSheep:
// the visible BODY is ModelSheep2 (skin sheep.png, full-height legs), and an
// inflated WOOL layer is ModelSheep1 (skin sheep_fur.png) drawn over it only
// when not sheared. We render white wool untinted (colored fleece/dyeing is a
// later milestone). SetVariant(1) hides the wool (a sheared sheep). Both layers
// share the same part structure, so one set of part indices drives both.
class SheepModel : public IMobModel {
public:
    SheepModel();

    bool Ready() const override { return m_body.HasSkin(); }

    // 0 = woolly (default), 1 = sheared (hide the wool layer).
    void SetVariant(int variant) override { m_sheared = (variant != 0); }

    void SetRotationAngles(float limbSwing, float limbSwingAmount, float age, float headYawDeg,
                           float headPitchDeg) override;

    void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const override;

private:
    vox::BoxModel m_body; // ModelSheep2 (sheep.png)
    vox::BoxModel m_wool; // ModelSheep1, inflated (sheep_fur.png)
    bool m_sheared = false;
    int m_head = -1;
    int m_bodyPart = -1;
    int m_leg[4] = {-1, -1, -1, -1};
};

} // namespace vc
