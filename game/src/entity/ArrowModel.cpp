#include "entity/ArrowModel.h"

#include <filesystem>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assets.h"
#include "vox/renderer/Buffer.h"
#include "vox/renderer/Renderer.h"

namespace vc {

namespace {

// One textured quad: 4 corners (pixel/model space) + uv (vanilla image-space
// fractions of the 32x32 texture) + a normal. The Texture2D loader uploads
// bottom-row-first, so GL v = 1 - imageV (matching BoxModel's unwrap).
struct Quad {
    glm::vec3 p[4];
    glm::vec2 uv[4];
    glm::vec3 n;
};

void AppendQuad(std::vector<float>& verts, std::vector<uint32_t>& indices, const Quad& q) {
    const auto base = static_cast<uint32_t>(verts.size() / 8);
    for (int c = 0; c < 4; ++c) {
        verts.insert(verts.end(), {q.p[c].x, q.p[c].y, q.p[c].z, q.n.x, q.n.y, q.n.z, q.uv[c].x,
                                   1.0f - q.uv[c].y});
    }
    indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
}

} // namespace

ArrowModel::ArrowModel() {
    std::vector<float> verts;
    std::vector<uint32_t> indices;

    // The two fletching cross-planes at the tail (x = -7), front + back faces
    // (vanilla RenderArrow's first two quads).
    AppendQuad(verts, indices,
               {{{-7, -2, -2}, {-7, -2, 2}, {-7, 2, 2}, {-7, 2, -2}},
                {{0.0f, 0.15625f}, {0.15625f, 0.15625f}, {0.15625f, 0.3125f}, {0.0f, 0.3125f}},
                {1, 0, 0}});
    AppendQuad(verts, indices,
               {{{-7, 2, -2}, {-7, 2, 2}, {-7, -2, 2}, {-7, -2, -2}},
                {{0.0f, 0.15625f}, {0.15625f, 0.15625f}, {0.15625f, 0.3125f}, {0.0f, 0.3125f}},
                {-1, 0, 0}});

    // The shaft: vanilla draws one quad in the z=0 plane four times, rotating
    // 90 deg about X each pass, forming a + cross-prism. Bake the four rotations.
    const Quad shaftBase{{{-8, -2, 0}, {8, -2, 0}, {8, 2, 0}, {-8, 2, 0}},
                         {{0.0f, 0.0f}, {0.5f, 0.0f}, {0.5f, 0.15625f}, {0.0f, 0.15625f}},
                         {0, 0, 1}};
    for (int k = 1; k <= 4; ++k) {
        const glm::mat4 r = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f * static_cast<float>(k)),
                                        {1.0f, 0.0f, 0.0f});
        Quad q = shaftBase;
        for (int c = 0; c < 4; ++c) {
            q.p[c] = glm::vec3(r * glm::vec4(shaftBase.p[c], 1.0f));
        }
        q.n = glm::vec3(r * glm::vec4(shaftBase.n, 0.0f));
        AppendQuad(verts, indices, q);
    }

    auto buffer = std::make_shared<vox::VertexBuffer>(
        verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float)));
    buffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                       {vox::ShaderDataType::Float3, "a_normal"},
                       {vox::ShaderDataType::Float2, "a_uv"}});
    m_mesh = std::make_shared<vox::VertexArray>();
    m_mesh->AddVertexBuffer(std::move(buffer));
    m_mesh->SetIndexBuffer(
        std::make_shared<vox::IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size())));

    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/entity/projectiles/arrow.png"))) {
        m_skin = vox::Texture2D::FromFile("mc/textures/entity/projectiles/arrow.png");
    }
}

void ArrowModel::Render(vox::Shader& shader, const glm::mat4& modelToWorld, int skinUnit) const {
    if (!m_skin) {
        return;
    }
    m_skin->Bind(static_cast<uint32_t>(skinUnit));
    shader.SetMat4("u_model", modelToWorld);
    vox::Renderer::DrawIndexed(*m_mesh);
}

} // namespace vc
