#include "entity/HeldBowModel.h"

#include <filesystem>
#include <vector>

#include "vox/core/Assets.h"
#include "vox/renderer/Buffer.h"
#include "vox/renderer/Renderer.h"

namespace vc {

HeldBowModel::HeldBowModel() {
    // A 16x16 quad centered on the origin in the XY plane, emitted with both
    // windings so it shows from either side (the entity pass renders cull-off
    // anyway). Layout matches BoxModel/ArrowModel: pos(3) + normal(3) + uv(2).
    // The Texture2D loader uploads bottom-row-first, so v = 1 - imageV.
    const float h = 8.0f; // half-size (pixels)
    const float quad[] = {
        // front (+Z)
        -h, -h, 0, 0, 0, 1, 0, 0,  h, -h, 0, 0, 0, 1, 1, 0,
         h,  h, 0, 0, 0, 1, 1, 1, -h,  h, 0, 0, 0, 1, 0, 1,
        // back (-Z)
        -h, -h, 0, 0, 0, -1, 0, 0, -h,  h, 0, 0, 0, -1, 0, 1,
         h,  h, 0, 0, 0, -1, 1, 1,  h, -h, 0, 0, 0, -1, 1, 0,
    };
    const uint32_t idx[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

    auto buffer = std::make_shared<vox::VertexBuffer>(quad, static_cast<uint32_t>(sizeof(quad)));
    buffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                       {vox::ShaderDataType::Float3, "a_normal"},
                       {vox::ShaderDataType::Float2, "a_uv"}});
    m_mesh = std::make_shared<vox::VertexArray>();
    m_mesh->AddVertexBuffer(std::move(buffer));
    m_mesh->SetIndexBuffer(
        std::make_shared<vox::IndexBuffer>(idx, static_cast<uint32_t>(std::size(idx))));

    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/items/bow_standby.png"))) {
        m_skin = vox::Texture2D::FromFile("mc/textures/items/bow_standby.png");
    }
}

void HeldBowModel::Render(vox::Shader& shader, const glm::mat4& modelToWorld, int skinUnit) const {
    if (!m_skin) {
        return;
    }
    m_skin->Bind(static_cast<uint32_t>(skinUnit));
    shader.SetMat4("u_model", modelToWorld);
    vox::Renderer::DrawIndexed(*m_mesh);
}

} // namespace vc
