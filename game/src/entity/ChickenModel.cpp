#include "entity/ChickenModel.h"

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

ChickenModel::ChickenModel() {
    using Box = vox::BoxModel::Box;
    // Vanilla ModelChicken, verbatim boxes/pivots/texOffsets. Pixels, Y-down,
    // 64x32 skin. Head/bill/chin share the (0,15,-4) pivot; body is rotated
    // upright per frame; legs at pivot y19 + 5px box put the feet at y24.
    m_head = m_model.AddPart("head", {0.0f, 15.0f, -4.0f},
                             {Box{{-2, -6, -2}, {4, 6, 3}, {0, 0}, 0.0f, false}});
    m_bill = m_model.AddPart("bill", {0.0f, 15.0f, -4.0f},
                             {Box{{-2, -4, -4}, {4, 2, 2}, {14, 0}, 0.0f, false}});
    m_chin = m_model.AddPart("chin", {0.0f, 15.0f, -4.0f},
                             {Box{{-1, -2, -3}, {2, 2, 2}, {14, 4}, 0.0f, false}});
    m_body = m_model.AddPart("body", {0.0f, 16.0f, 0.0f},
                             {Box{{-3, -4, -3}, {6, 8, 6}, {0, 9}, 0.0f, false}});
    m_rightLeg = m_model.AddPart("rightLeg", {-2.0f, 19.0f, 1.0f},
                                 {Box{{-1, 0, -3}, {3, 5, 3}, {26, 0}, 0.0f, false}});
    m_leftLeg = m_model.AddPart("leftLeg", {1.0f, 19.0f, 1.0f},
                                {Box{{-1, 0, -3}, {3, 5, 3}, {26, 0}, 0.0f, false}});
    m_rightWing = m_model.AddPart("rightWing", {-4.0f, 13.0f, 0.0f},
                                  {Box{{0, 0, -3}, {1, 4, 6}, {24, 13}, 0.0f, false}});
    m_leftWing = m_model.AddPart("leftWing", {4.0f, 13.0f, 0.0f},
                                 {Box{{-1, 0, -3}, {1, 4, 6}, {24, 13}, 0.0f, false}});

    const char* skin = "mc/textures/entity/chicken/chicken.png";
    if (std::filesystem::exists(vox::assets::Resolve(skin))) {
        m_model.SetSkin(vox::Texture2D::FromFile(skin), 64.0f, 32.0f);
    }
    m_model.Build();
}

void ChickenModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float age,
                                     float headYawDeg, float headPitchDeg) {
    // ModelChicken.setRotationAngles: head/bill/chin share the head angles, the
    // body sits upright, the legs trot in opposite phase. Vanilla flaps the
    // wings by the entity's accumulating wingRotation; we don't sim that, so we
    // give a gentle sine idle flap off `age` (a falling-chicken fast flap is a
    // backlog nicety).
    const glm::vec3 headRot{headPitchDeg * kDegToRad, headYawDeg * kDegToRad, 0.0f};
    m_model.SetRotation(m_head, headRot);
    m_model.SetRotation(m_bill, headRot);
    m_model.SetRotation(m_chin, headRot);
    m_model.SetRotation(m_body, {kPi / 2.0f, 0.0f, 0.0f});

    const float swing = limbSwing * 0.6662f;
    m_model.SetRotation(m_rightLeg, {std::cos(swing) * 1.4f * limbSwingAmount, 0.0f, 0.0f});
    m_model.SetRotation(m_leftLeg, {std::cos(swing + kPi) * 1.4f * limbSwingAmount, 0.0f, 0.0f});

    const float flap = std::sin(age * 0.3f) * 0.3f;
    m_model.SetRotation(m_rightWing, {0.0f, 0.0f, flap});
    m_model.SetRotation(m_leftWing, {0.0f, 0.0f, -flap});
}

} // namespace vc
