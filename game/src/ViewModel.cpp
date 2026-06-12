#include "ViewModel.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assets.h"
#include "vox/renderer/Renderer.h"

namespace vc {

namespace {

constexpr float kPi = glm::pi<float>();
constexpr int kSwingTicks = 6; // vanilla getArmSwingAnimationEnd

glm::mat4 RotX(const glm::mat4& m, float deg) {
    return glm::rotate(m, glm::radians(deg), {1.0f, 0.0f, 0.0f});
}
glm::mat4 RotY(const glm::mat4& m, float deg) {
    return glm::rotate(m, glm::radians(deg), {0.0f, 1.0f, 0.0f});
}
glm::mat4 RotZ(const glm::mat4& m, float deg) {
    return glm::rotate(m, glm::radians(deg), {0.0f, 0.0f, 1.0f});
}

// Steve's right arm as authored in ModelBiped: a 4x12x4-px box at
// (-3,-2,-2) with rotation point (-5,2,0), model scale 1/16, Y-DOWN model
// space (the vanilla arm matrix chain flips it into view). Skin UVs from
// the classic 64x64 layout at texOffs(40,16); the loader flips textures,
// so v = 1 - imageV/64.
std::shared_ptr<vox::VertexArray> BuildArmMesh() {
    constexpr float s = 0.0625f;
    const glm::vec3 lo = (glm::vec3{-3.0f, -2.0f, -2.0f} + glm::vec3{-5.0f, 2.0f, 0.0f}) * s;
    const glm::vec3 hi = (glm::vec3{1.0f, 10.0f, 2.0f} + glm::vec3{-5.0f, 2.0f, 0.0f}) * s;

    struct Face {
        glm::vec3 corners[4]; // CCW from the face's lower-left in uv space
        glm::vec3 normal;
        float u, v, w, h; // image-pixel rect in the 64x64 skin
    };
    const Face faces[] = {
        // +X (inner), -X (outer), +Y (hand end, y-down model!), -Y
        // (shoulder), +Z (back), -Z (front) — classic arm layout columns
        // 40/44/48/52 at row 20, caps at row 16.
        {{{hi.x, lo.y, lo.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {hi.x, hi.y, lo.z}},
         {1, 0, 0}, 48, 20, 4, 12},
        {{{lo.x, lo.y, hi.z}, {lo.x, lo.y, lo.z}, {lo.x, hi.y, lo.z}, {lo.x, hi.y, hi.z}},
         {-1, 0, 0}, 40, 20, 4, 12},
        {{{lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}},
         {0, 1, 0}, 48, 16, 4, 4},
        {{{lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, lo.y, lo.z}, {lo.x, lo.y, lo.z}},
         {0, -1, 0}, 44, 16, 4, 4},
        {{{hi.x, lo.y, hi.z}, {lo.x, lo.y, hi.z}, {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z}},
         {0, 0, 1}, 52, 20, 4, 12},
        {{{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z}},
         {0, 0, -1}, 44, 20, 4, 12},
    };

    std::vector<float> verts;
    std::vector<uint32_t> indices;
    for (const Face& f : faces) {
        const uint32_t base = static_cast<uint32_t>(verts.size() / 9);
        // The model is y-down: image-top of the rect maps to the box's
        // -y end, which the chain's Rx(200) turns upright.
        const glm::vec2 uvs[4] = {
            {f.u / 64.0f, 1.0f - (f.v + f.h) / 64.0f},
            {(f.u + f.w) / 64.0f, 1.0f - (f.v + f.h) / 64.0f},
            {(f.u + f.w) / 64.0f, 1.0f - f.v / 64.0f},
            {f.u / 64.0f, 1.0f - f.v / 64.0f},
        };
        for (int c = 0; c < 4; ++c) {
            verts.insert(verts.end(),
                         {f.corners[c].x, f.corners[c].y, f.corners[c].z, f.normal.x,
                          f.normal.y, f.normal.z, uvs[c].x, uvs[c].y, 0.0f});
        }
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base + 2, base + 3, base});
    }

    auto buffer = std::make_shared<vox::VertexBuffer>(
        verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float)));
    buffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                       {vox::ShaderDataType::Float3, "a_normal"},
                       {vox::ShaderDataType::Float2, "a_uv"},
                       {vox::ShaderDataType::Float, "a_face"}});
    auto vao = std::make_shared<vox::VertexArray>();
    vao->AddVertexBuffer(std::move(buffer));
    vao->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
        indices.data(), static_cast<uint32_t>(indices.size())));
    return vao;
}

} // namespace

ViewModel::ViewModel(std::shared_ptr<vox::VertexArray> cube,
                     std::shared_ptr<vox::VertexArray> quad)
    : m_cube(std::move(cube)), m_quad(std::move(quad)) {
    m_shader = vox::Shader::FromFiles("shaders/viewmodel.vert", "shaders/viewmodel.frag");
    m_arm = BuildArmMesh();
    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/entity/steve.png"))) {
        m_skin = vox::Texture2D::FromFile("mc/textures/entity/steve.png");
    }
}

void ViewModel::Tick(const ItemStack& held) {
    // Swing: 6 ticks; a queued trigger restarts immediately on finish, so
    // held digging reads as continuous swinging.
    m_prevProgress = m_progress;
    if (m_swingTicks < 0 && m_swingQueued) {
        m_swingTicks = 0;
        m_swingQueued = false;
        m_prevProgress = 0.0f;
    }
    if (m_swingTicks >= 0) {
        ++m_swingTicks;
        if (m_swingTicks >= kSwingTicks) {
            m_swingTicks = -1;
            m_progress = 0.0f;
            m_prevProgress = 0.0f;
        } else {
            m_progress = static_cast<float>(m_swingTicks) / kSwingTicks;
        }
    }

    // Equip dip (vanilla updateEquippedItem): slide toward 1 while the
    // hand holds the displayed item, toward 0 otherwise; swap what is
    // displayed at the bottom of the dip.
    m_prevEquip = m_equip;
    const ItemId heldId = held.Empty() ? static_cast<ItemId>(blocks::Air) : held.id;
    const bool same = heldId == m_displayId;
    m_equip += std::clamp((same ? 1.0f : 0.0f) - m_equip, -0.4f, 0.4f);
    if (!same && m_equip < 0.05f) {
        m_displayId = heldId;
    }
}

void ViewModel::Render(const vox::PerspectiveCamera& camera, double alpha, glm::vec2 light,
                       float sunLight, const glm::vec3& skyTint) {
    const bool arm = m_displayId == blocks::Air;
    if (arm && !m_skin) {
        return; // no imported skin: bare hand draws nothing (placeholder)
    }
    const float a = static_cast<float>(alpha);
    const float sw = m_swingTicks >= 0 ? glm::mix(m_prevProgress, m_progress, a) : 0.0f;
    const float lower = 1.0f - glm::mix(m_prevEquip, m_equip, a);
    const float sqrtSw = std::sqrt(sw);

    // The hand draws over everything and must not poke into walls.
    vox::Renderer::ClearDepth();
    vox::Renderer::SetCullFace(false);
    m_shader->Bind();
    m_shader->SetMat4("u_proj", camera.Projection());
    m_shader->SetInt("u_atlas", 0);
    m_shader->SetInt("u_skin", 1);
    m_shader->SetFloat2("u_light", light);
    m_shader->SetFloat("u_sunLight", sunLight);
    m_shader->SetFloat3("u_skyTint", skyTint);

    glm::mat4 m{1.0f};
    if (arm) {
        // ItemRenderer.renderArmFirstPerson, right hand, verbatim.
        m = glm::translate(m, {-0.3f * std::sin(sqrtSw * kPi) + 0.64f,
                               0.4f * std::sin(sqrtSw * 2.0f * kPi) - 0.6f - 0.6f * lower,
                               -0.4f * std::sin(sw * kPi) - 0.72f});
        m = RotY(m, 45.0f);
        m = RotY(m, std::sin(sqrtSw * kPi) * 70.0f);
        m = RotZ(m, std::sin(sw * sw * kPi) * -20.0f);
        m = glm::translate(m, {-1.0f, 3.6f, 3.5f});
        m = RotZ(m, 120.0f);
        m = RotX(m, 200.0f);
        m = RotY(m, -135.0f);
        m = glm::translate(m, {5.6f, 0.0f, 0.0f});
        m_shader->SetMat4("u_model", m);
        m_shader->SetFloat("u_useSkin", 1.0f);
        m_skin->Bind(1);
        vox::Renderer::DrawIndexed(*m_arm);
    } else {
        // ItemRenderer.renderItemInFirstPerson (no use-actions), right
        // hand: swing offset, side transform, swing rotations, then the
        // model's first-person display transform.
        m = glm::translate(m, {-0.4f * std::sin(sqrtSw * kPi),
                               0.2f * std::sin(sqrtSw * 2.0f * kPi),
                               -0.2f * std::sin(sw * kPi)});
        m = glm::translate(m, {0.56f, -0.52f - 0.6f * lower, -0.72f});
        m = RotY(m, 45.0f + std::sin(sw * sw * kPi) * -20.0f);
        const float f1 = std::sin(sqrtSw * kPi);
        m = RotZ(m, f1 * -20.0f);
        m = RotX(m, f1 * -80.0f);
        m = RotY(m, -45.0f);

        const bool block = IsBlockItem(m_displayId);
        if (block) {
            // block.json firstperson_righthand: rotate Y 45, scale 0.40.
            m = RotY(m, 45.0f);
            m = glm::scale(m, glm::vec3{0.40f});
        } else {
            // item/generated firstperson_righthand: T(1.13,3.2,1.13)/16,
            // rotate [0,-90,25], scale 0.68.
            m = glm::translate(m, glm::vec3{1.13f, 3.2f, 1.13f} / 16.0f);
            m = RotY(m, -90.0f);
            m = RotZ(m, 25.0f);
            m = glm::scale(m, glm::vec3{0.68f});
        }
        m = glm::translate(m, glm::vec3{-0.5f}); // model spans 0..1, center it

        std::array<uint16_t, 6> tiles;
        if (block) {
            tiles = BlockRegistry::Get().Def(m_displayId).faceTiles;
        } else {
            tiles.fill(ItemIconTile(m_displayId));
        }
        for (int face = 0; face < 6; ++face) {
            m_shader->SetFloat(std::format("u_faceLayers[{}]", face),
                               static_cast<float>(tiles[static_cast<size_t>(face)]));
        }
        m_shader->SetMat4("u_model", m);
        m_shader->SetFloat("u_useSkin", 0.0f);
        vox::Renderer::DrawIndexed(block ? *m_cube : *m_quad);
    }
    vox::Renderer::SetCullFace(true);
}

} // namespace vc
