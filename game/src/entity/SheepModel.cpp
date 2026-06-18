#include "entity/SheepModel.h"

#include <cmath>
#include <filesystem>
#include <string>

#include <glm/gtc/constants.hpp>

#include "vox/core/Assets.h"
#include "vox/renderer/Texture.h"

namespace vc {

namespace {
constexpr float kPi = glm::pi<float>();
constexpr float kDegToRad = kPi / 180.0f;

// Shared quadruped leg pivots (ModelSheep1/2, both inherit ModelQuadruped(12,0)
// rotation points). Pixels, Y-down. Front legs at z=-5, back at z=7.
const glm::vec3 kLegPivots[4] = {
    {-3.0f, 12.0f, 7.0f},  // back-right
    {3.0f, 12.0f, 7.0f},   // back-left
    {-3.0f, 12.0f, -5.0f}, // front-right
    {3.0f, 12.0f, -5.0f},  // front-left
};
} // namespace

SheepModel::SheepModel() {
    using Box = vox::BoxModel::Box;

    // --- Body layer: ModelSheep2 (no inflate, full 12px legs; sheep.png) ------
    m_head = m_body.AddPart("head", {0.0f, 6.0f, -8.0f},
                            {Box{{-3, -4, -6}, {6, 6, 8}, {0, 0}, 0.0f, false}});
    m_bodyPart = m_body.AddPart("body", {0.0f, 5.0f, 2.0f},
                                {Box{{-4, -10, -7}, {8, 16, 6}, {28, 8}, 0.0f, false}});
    for (int i = 0; i < 4; ++i) {
        m_leg[i] = m_body.AddPart("leg" + std::to_string(i), kLegPivots[i],
                                  {Box{{-2, 0, -2}, {4, 12, 4}, {0, 16}, 0.0f, false}});
    }
    const char* bodySkin = "mc/textures/entity/sheep/sheep.png";
    if (std::filesystem::exists(vox::assets::Resolve(bodySkin))) {
        m_body.SetSkin(vox::Texture2D::FromFile(bodySkin), 64.0f, 32.0f);
    }
    m_body.Build();

    // --- Wool layer: ModelSheep1 (inflated head/body/legs; sheep_fur.png) -----
    // Added in the SAME part order so the indices above drive both layers.
    m_wool.AddPart("head", {0.0f, 6.0f, -8.0f},
                   {Box{{-3, -4, -4}, {6, 6, 6}, {0, 0}, 0.6f, false}});
    m_wool.AddPart("body", {0.0f, 5.0f, 2.0f},
                   {Box{{-4, -10, -7}, {8, 16, 6}, {28, 8}, 1.75f, false}});
    for (int i = 0; i < 4; ++i) {
        m_wool.AddPart("leg" + std::to_string(i), kLegPivots[i],
                       {Box{{-2, 0, -2}, {4, 6, 4}, {0, 16}, 0.5f, false}});
    }
    const char* woolSkin = "mc/textures/entity/sheep/sheep_fur.png";
    if (std::filesystem::exists(vox::assets::Resolve(woolSkin))) {
        m_wool.SetSkin(vox::Texture2D::FromFile(woolSkin), 64.0f, 32.0f);
    }
    m_wool.Build();
}

void SheepModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float /*age*/,
                                   float headYawDeg, float headPitchDeg) {
    // ModelQuadruped.setRotationAngles applied to BOTH layers (same indices).
    const glm::vec3 head{headPitchDeg * kDegToRad, headYawDeg * kDegToRad, 0.0f};
    const glm::vec3 body{kPi / 2.0f, 0.0f, 0.0f};
    const float swing = limbSwing * 0.6662f;
    const float a = std::cos(swing) * 1.4f * limbSwingAmount;
    const float b = std::cos(swing + kPi) * 1.4f * limbSwingAmount;
    const glm::vec3 legRot[4] = {{a, 0, 0}, {b, 0, 0}, {b, 0, 0}, {a, 0, 0}};

    for (vox::BoxModel* m : {&m_body, &m_wool}) {
        m->SetRotation(m_head, head);
        m->SetRotation(m_bodyPart, body);
        for (int i = 0; i < 4; ++i) {
            m->SetRotation(m_leg[i], legRot[i]);
        }
    }
}

void SheepModel::Render(vox::Shader& shader, const glm::mat4& modelToWorld) const {
    m_body.Render(shader, modelToWorld);
    if (!m_sheared && m_wool.HasSkin()) {
        m_wool.Render(shader, modelToWorld);
    }
}

} // namespace vc
