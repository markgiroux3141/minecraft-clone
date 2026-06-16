#include "entity/PigModel.h"

#include <cmath>
#include <filesystem>

#include <glm/gtc/constants.hpp>

#include "vox/core/Assets.h"
#include "vox/renderer/Texture.h"

namespace vc {

namespace {
constexpr float kPi = glm::pi<float>();
constexpr float kDegToRad = kPi / 180.0f;
} // namespace

PigModel::PigModel() {
    using Box = vox::BoxModel::Box;
    // Vanilla ModelQuadruped(headOffset=6, scale=0) + ModelPig's snout. Pixels,
    // Y-down model space, 64x32 skin. Pivots (rotationPoint) and box origins
    // (part-local) are verbatim from the vanilla constructors.
    m_head = m_model.AddPart(
        "head", {0.0f, 12.0f, -6.0f},
        {Box{{-4, -4, -8}, {8, 8, 8}, {0, 0}, 0.0f, false},
         // ModelPig snout: head.setTextureOffset(16,16).addBox(-2,0,-9,4,3,1).
         Box{{-2, 0, -9}, {4, 3, 1}, {16, 16}, 0.0f, false}});
    // Body lies horizontal (rotateAngleX = pi/2, set per frame). Vanilla's
    // rotationPoint Y is 17, but our renderer's upright flip (Rx(pi) + the 24px
    // feet offset) maps the rotated box to world-y = 24 - (pivotY + rotatedY),
    // so 17 drops the body onto the ground with the legs buried inside it.
    // Pivot Y 12 lifts the body to sit on top of the 6px legs (with a 1px
    // overlap so there's no seam) — the snout/head then read correctly too.
    m_body = m_model.AddPart("body", {0.0f, 12.0f, 2.0f},
                             {Box{{-5, -10, -7}, {10, 16, 8}, {28, 8}, 0.0f, false}});
    // Four legs, 6 px tall (height == headOffset), all sharing texOffset (0,16).
    const glm::vec3 legPivots[4] = {
        {-3.0f, 18.0f, 7.0f},  // back-right
        {3.0f, 18.0f, 7.0f},   // back-left
        {-3.0f, 18.0f, -5.0f}, // front-right
        {3.0f, 18.0f, -5.0f},  // front-left
    };
    for (int i = 0; i < 4; ++i) {
        m_leg[i] = m_model.AddPart("leg" + std::to_string(i), legPivots[i],
                                   {Box{{-2, 0, -2}, {4, 6, 4}, {0, 16}, 0.0f, false}});
    }

    const char* skin = "mc/textures/entity/pig/pig.png";
    if (std::filesystem::exists(vox::assets::Resolve(skin))) {
        m_model.SetSkin(vox::Texture2D::FromFile(skin), 64.0f, 32.0f);
    }
    m_model.Build();
}

void PigModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float /*age*/,
                                 float headYawDeg, float headPitchDeg) {
    // ModelQuadruped.setRotationAngles, verbatim. Diagonal leg pairs trot in
    // opposite phase; the body stays horizontal.
    m_model.SetRotation(m_head, {headPitchDeg * kDegToRad, headYawDeg * kDegToRad, 0.0f});
    m_model.SetRotation(m_body, {kPi / 2.0f, 0.0f, 0.0f});

    const float swing = limbSwing * 0.6662f;
    const float a = std::cos(swing) * 1.4f * limbSwingAmount;
    const float b = std::cos(swing + kPi) * 1.4f * limbSwingAmount;
    m_model.SetRotation(m_leg[0], {a, 0.0f, 0.0f}); // back-right
    m_model.SetRotation(m_leg[1], {b, 0.0f, 0.0f}); // back-left
    m_model.SetRotation(m_leg[2], {b, 0.0f, 0.0f}); // front-right
    m_model.SetRotation(m_leg[3], {a, 0.0f, 0.0f}); // front-left
}

} // namespace vc
