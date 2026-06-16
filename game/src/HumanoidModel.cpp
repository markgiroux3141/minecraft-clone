#include "HumanoidModel.h"

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

HumanoidModel::HumanoidModel(std::string skinRel, bool zombiePose) : m_zombiePose(zombiePose) {
    using Box = vox::BoxModel::Box;
    // Vanilla ModelBiped boxes (pixels, Y-down model space, 64x64 skin). The
    // origin is part-local (relative to the pivot/rotationPoint). Pivots and
    // texture offsets are verbatim from ModelBiped's constructor.
    m_head = m_model.AddPart("head", {0.0f, 0.0f, 0.0f},
                             {Box{{-4, -8, -4}, {8, 8, 8}, {0, 0}, 0.0f, false},
                              // bipedHeadwear: same box, +0.5 inflate, texOffset(32,0).
                              Box{{-4, -8, -4}, {8, 8, 8}, {32, 0}, 0.5f, false}});
    m_body = m_model.AddPart("body", {0.0f, 0.0f, 0.0f},
                             {Box{{-4, 0, -2}, {8, 12, 4}, {16, 16}, 0.0f, false}});
    m_rightArm = m_model.AddPart("rightArm", {-5.0f, 2.0f, 0.0f},
                                 {Box{{-3, -2, -2}, {4, 12, 4}, {40, 16}, 0.0f, false}});
    m_leftArm = m_model.AddPart("leftArm", {5.0f, 2.0f, 0.0f},
                                {Box{{-1, -2, -2}, {4, 12, 4}, {40, 16}, 0.0f, true}});
    m_rightLeg = m_model.AddPart("rightLeg", {-1.9f, 12.0f, 0.0f},
                                 {Box{{-2, 0, -2}, {4, 12, 4}, {0, 16}, 0.0f, false}});
    m_leftLeg = m_model.AddPart("leftLeg", {1.9f, 12.0f, 0.0f},
                                {Box{{-2, 0, -2}, {4, 12, 4}, {0, 16}, 0.0f, true}});

    if (std::filesystem::exists(vox::assets::Resolve(skinRel))) {
        m_model.SetSkin(vox::Texture2D::FromFile(skinRel), 64.0f, 64.0f);
    }
    m_model.Build();
}

void HumanoidModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float age,
                                      float headYawDeg, float headPitchDeg) {
    // ModelBiped.setRotationAngles, verbatim. Head tracks look; arms/legs
    // swing on the walk cycle (0.6662 frequency; arms ±π out of phase from the
    // opposite limb), then the arms get the constant idle sway.
    m_model.SetRotation(m_head, {headPitchDeg * kDegToRad, headYawDeg * kDegToRad, 0.0f});

    const float swing = limbSwing * 0.6662f;
    const float rightArmX = std::cos(swing + kPi) * 2.0f * limbSwingAmount * 0.5f;
    const float leftArmX = std::cos(swing) * 2.0f * limbSwingAmount * 0.5f;
    const float idleZ = std::cos(age * 0.09f) * 0.05f + 0.05f;
    const float idleX = std::sin(age * 0.067f) * 0.05f;
    if (m_zombiePose) {
        // Vanilla ModelZombie: arms held straight out (rotateAngleX ~ -pi/2),
        // toed slightly inward, bobbing as it walks — the classic shamble.
        const float arm = -kPi / 2.0f + std::sin(age * 0.067f) * 0.05f;
        const float armSwing = std::cos(swing) * 0.4f * limbSwingAmount;
        m_model.SetRotation(m_rightArm, {arm - armSwing, 0.0f, -0.05f});
        m_model.SetRotation(m_leftArm, {arm + armSwing, 0.0f, 0.05f});
    } else {
        m_model.SetRotation(m_rightArm, {rightArmX + idleX, 0.0f, idleZ});
        m_model.SetRotation(m_leftArm, {leftArmX - idleX, 0.0f, -idleZ});
    }

    m_model.SetRotation(m_rightLeg, {std::cos(swing) * 1.4f * limbSwingAmount, 0.0f, 0.0f});
    m_model.SetRotation(m_leftLeg, {std::cos(swing + kPi) * 1.4f * limbSwingAmount, 0.0f, 0.0f});

    m_model.SetRotation(m_body, {0.0f, 0.0f, 0.0f});
}

} // namespace vc
