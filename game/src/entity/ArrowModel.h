#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"
#include "vox/renderer/VertexArray.h"

namespace vc {

// M36: the flying arrow's render model — a verbatim port of vanilla RenderArrow
// (a cross-prism shaft + a fletching cross), skinned from the 32x32
// entity/projectiles/arrow.png. Drawn with the entity_model shader like mobs.
// The geometry is authored in vanilla "pixel" units along +X (the arrowhead
// points toward +X); the caller's modelToWorld folds in the yaw/pitch from the
// arrow's velocity and the vanilla 0.05625 scale. A clean clone without the mc
// asset overlay draws nothing (like the mobs / first-person arm).
class ArrowModel {
public:
    ArrowModel();

    bool Ready() const { return m_skin != nullptr; }

    // Binds the arrow skin to unit `skinUnit`, sets u_model, draws. The caller
    // has bound the entity_model shader + its view/proj + lighting uniforms.
    void Render(vox::Shader& shader, const glm::mat4& modelToWorld, int skinUnit = 1) const;

private:
    std::shared_ptr<vox::VertexArray> m_mesh;
    std::shared_ptr<vox::Texture2D> m_skin; // null without assets/mc overlay
};

} // namespace vc
