#include "entity/CreeperModel.h"

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
} // namespace

CreeperModel::CreeperModel() {
    using Box = vox::BoxModel::Box;
    // Verbatim boxes/pivots/texOffsets from the vanilla ModelCreeper constructor
    // (pixels, Y-down model space, 64x32 skin). Unlike the quadrupeds the body
    // stays upright (no pi/2 rotation).
    m_head = m_model.AddPart("head", {0.0f, 6.0f, 0.0f},
                             {Box{{-4, -8, -4}, {8, 8, 8}, {0, 0}, 0.0f, false}});
    m_body = m_model.AddPart("body", {0.0f, 6.0f, 0.0f},
                             {Box{{-4, 0, -2}, {8, 12, 4}, {16, 16}, 0.0f, false}});
    // Four 4x6x4 legs, all sharing texOffset (0,16). Pivots from ModelCreeper.
    const glm::vec3 legPivots[4] = {
        {-2.0f, 18.0f, 4.0f},  // leg1 (back-right)
        {2.0f, 18.0f, 4.0f},   // leg2 (back-left)
        {-2.0f, 18.0f, -4.0f}, // leg3 (front-right)
        {2.0f, 18.0f, -4.0f},  // leg4 (front-left)
    };
    for (int i = 0; i < 4; ++i) {
        m_leg[i] = m_model.AddPart("leg" + std::to_string(i), legPivots[i],
                                   {Box{{-2, 0, -2}, {4, 6, 4}, {0, 16}, 0.0f, false}});
    }

    const char* skin = "mc/textures/entity/creeper/creeper.png";
    if (std::filesystem::exists(vox::assets::Resolve(skin))) {
        m_model.SetSkin(vox::Texture2D::FromFile(skin), 64.0f, 32.0f);
    }
    m_model.Build();
}

void CreeperModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float /*age*/,
                                     float headYawDeg, float headPitchDeg) {
    // ModelCreeper.setRotationAngles, verbatim: head tracks yaw/pitch; the four
    // legs trot in two diagonal phases (leg1+leg4 vs leg2+leg3).
    m_model.SetRotation(m_head, {headPitchDeg * kDegToRad, headYawDeg * kDegToRad, 0.0f});

    const float swing = limbSwing * 0.6662f;
    const float a = std::cos(swing) * 1.4f * limbSwingAmount;
    const float b = std::cos(swing + kPi) * 1.4f * limbSwingAmount;
    m_model.SetRotation(m_leg[0], {a, 0.0f, 0.0f}); // back-right
    m_model.SetRotation(m_leg[1], {b, 0.0f, 0.0f}); // back-left
    m_model.SetRotation(m_leg[2], {b, 0.0f, 0.0f}); // front-right
    m_model.SetRotation(m_leg[3], {a, 0.0f, 0.0f}); // front-left
}

} // namespace vc
