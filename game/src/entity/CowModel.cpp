#include "entity/CowModel.h"

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

CowModel::CowModel() {
    using Box = vox::BoxModel::Box;
    // Vanilla ModelCow extends ModelQuadruped(12, 0) then rebuilds head/body and
    // nudges the legs out. Pixels, Y-down model space, 64x32 skin. Verbatim
    // boxes/pivots/texOffsets from the ModelCow constructor.
    m_head = m_model.AddPart(
        "head", {0.0f, 4.0f, -8.0f},
        {Box{{-4, -4, -6}, {8, 8, 6}, {0, 0}, 0.0f, false},
         // Two horns (texOffset 22,0 each).
         Box{{-5, -5, -4}, {1, 3, 1}, {22, 0}, 0.0f, false},
         Box{{4, -5, -4}, {1, 3, 1}, {22, 0}, 0.0f, false}});
    // Body lies horizontal (rotateAngleX = pi/2, set per frame), with a small
    // udder underneath. Pivot Y 5 = vanilla (17 - height 12); our upright flip
    // maps it correctly without the pig's adjustment because the deep body box
    // already spans the full standing height once rotated.
    m_body = m_model.AddPart("body", {0.0f, 5.0f, 2.0f},
                             {Box{{-6, -10, -7}, {12, 18, 10}, {18, 4}, 0.0f, false},
                              Box{{-2, 2, -8}, {4, 6, 1}, {52, 0}, 0.0f, false}});
    // Four 12px legs (height == headOffset 12), all sharing texOffset (0,16).
    // ModelCow pushes them out to x=+/-4 and the back legs to z=-6.
    const glm::vec3 legPivots[4] = {
        {-4.0f, 12.0f, 7.0f},  // back-right
        {4.0f, 12.0f, 7.0f},   // back-left
        {-4.0f, 12.0f, -6.0f}, // front-right
        {4.0f, 12.0f, -6.0f},  // front-left
    };
    for (int i = 0; i < 4; ++i) {
        m_leg[i] = m_model.AddPart("leg" + std::to_string(i), legPivots[i],
                                   {Box{{-2, 0, -2}, {4, 12, 4}, {0, 16}, 0.0f, false}});
    }

    const char* skin = "mc/textures/entity/cow/cow.png";
    if (std::filesystem::exists(vox::assets::Resolve(skin))) {
        m_model.SetSkin(vox::Texture2D::FromFile(skin), 64.0f, 32.0f);
    }
    m_model.Build();
}

void CowModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float /*age*/,
                                 float headYawDeg, float headPitchDeg) {
    // ModelQuadruped.setRotationAngles, verbatim (cow inherits it). Diagonal leg
    // pairs trot in opposite phase; the body stays horizontal.
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
