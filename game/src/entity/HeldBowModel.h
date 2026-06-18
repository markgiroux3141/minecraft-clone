#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"
#include "vox/renderer/VertexArray.h"

namespace vc {

// M36: the bow held in the skeleton's hand — a flat two-sided quad skinned from
// the 16x16 `items/bow_standby.png`, drawn with the entity_model shader at the
// skeleton's right-hand joint (HumanoidModel::RightArmTransform). The quad is
// authored in vanilla "pixel" units (16px, centered on origin) so it composes
// with the arm-local transform (which already folds in the model's 1/16 scale).
// A clean clone without the imported sprite draws nothing (clean-clone rule).
class HeldBowModel {
public:
    HeldBowModel();

    bool Ready() const { return m_skin != nullptr; }

    void Render(vox::Shader& shader, const glm::mat4& modelToWorld, int skinUnit = 1) const;

private:
    std::shared_ptr<vox::VertexArray> m_mesh;
    std::shared_ptr<vox::Texture2D> m_skin;
};

} // namespace vc
