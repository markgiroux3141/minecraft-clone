#include "entity/SpiderModel.h"

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

// Per-leg base rotations from ModelSpider.setRotationAngles (radians). Index i
// is vanilla spiderLeg(i+1): even i = right side (x=-4), odd i = left (x=+4).
constexpr float kLegBaseZ[8] = {-kPi / 4.0f, kPi / 4.0f, -0.58119464f, 0.58119464f,
                                -0.58119464f, 0.58119464f, -kPi / 4.0f, kPi / 4.0f};
constexpr float kLegBaseY[8] = {kPi / 4.0f,   -kPi / 4.0f, 0.3926991f,  -0.3926991f,
                                -0.3926991f,  0.3926991f,  -kPi / 4.0f, kPi / 4.0f};
} // namespace

SpiderModel::SpiderModel() {
    using Box = vox::BoxModel::Box;
    // Verbatim boxes / pivots / texOffsets from ModelSpider's constructor. Pixels,
    // Y-down model space, 64x32 skin. The head looks forward (-z), the body trails
    // behind (+z), and the eight legs anchor along the body sides.
    m_head = m_model.AddPart("head", {0.0f, 15.0f, -3.0f},
                             {Box{{-4, -4, -8}, {8, 8, 8}, {32, 4}, 0.0f, false}});
    m_neck = m_model.AddPart("neck", {0.0f, 15.0f, 0.0f},
                             {Box{{-3, -3, -3}, {6, 6, 6}, {0, 0}, 0.0f, false}});
    m_body = m_model.AddPart("body", {0.0f, 15.0f, 9.0f},
                             {Box{{-5, -4, -6}, {10, 8, 12}, {0, 12}, 0.0f, false}});

    // Eight 16x2x2 legs, all sharing texOffset (18,0). Right legs (even i, x=-4)
    // extend in -x (origin -15); left legs (odd i, x=+4) extend in +x (origin -1).
    // Pivot z steps 2,2,1,1,0,0,-1,-1 down the body (vanilla setRotationPoint).
    for (int i = 0; i < 8; ++i) {
        const bool right = (i % 2) == 0;
        const float px = right ? -4.0f : 4.0f;
        const float pz = 2.0f - static_cast<float>(i / 2);
        const glm::vec3 origin = right ? glm::vec3{-15.0f, -1.0f, -1.0f}
                                       : glm::vec3{-1.0f, -1.0f, -1.0f};
        m_leg[i] = m_model.AddPart("leg" + std::to_string(i), {px, 15.0f, pz},
                                   {Box{origin, {16, 2, 2}, {18, 0}, 0.0f, false}});
    }

    const char* skin = "mc/textures/entity/spider/spider.png";
    if (std::filesystem::exists(vox::assets::Resolve(skin))) {
        m_model.SetSkin(vox::Texture2D::FromFile(skin), 64.0f, 32.0f);
    }
    m_model.Build();
}

void SpiderModel::SetRotationAngles(float limbSwing, float limbSwingAmount, float /*age*/,
                                    float headYawDeg, float headPitchDeg) {
    // ModelSpider.setRotationAngles, verbatim. The head tracks yaw/pitch; the
    // eight legs hold their splayed base pose plus a four-phase scuttle.
    m_model.SetRotation(m_head, {headPitchDeg * kDegToRad, headYawDeg * kDegToRad, 0.0f});

    // Four phase offsets (0, pi, pi/2, 3pi/2), one per leg pair. fY swings the
    // legs fore/aft (cos at double frequency), fZ lifts them (abs sin).
    constexpr float kOff[4] = {0.0f, kPi, kPi / 2.0f, kPi * 3.0f / 2.0f};
    float fY[4];
    float fZ[4];
    for (int p = 0; p < 4; ++p) {
        fY[p] = -(std::cos(limbSwing * 0.6662f * 2.0f + kOff[p]) * 0.4f) * limbSwingAmount;
        fZ[p] = std::abs(std::sin(limbSwing * 0.6662f + kOff[p]) * 0.4f) * limbSwingAmount;
    }
    for (int i = 0; i < 8; ++i) {
        const int pair = i / 2;
        const float sign = (i % 2 == 0) ? 1.0f : -1.0f; // right side adds, left subtracts
        const float y = kLegBaseY[i] + sign * fY[pair];
        const float z = kLegBaseZ[i] + sign * fZ[pair];
        m_model.SetRotation(m_leg[i], {0.0f, y, z});
    }
}

} // namespace vc
