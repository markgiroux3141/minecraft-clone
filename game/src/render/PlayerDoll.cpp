#include "render/PlayerDoll.h"

#include <glm/gtc/matrix_transform.hpp>

#include "vox/renderer/Framebuffer.h"
#include "vox/renderer/Renderer.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"

namespace vc {

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Which biped parts each armor slot covers (vanilla LayerArmorBase visibility).
using Part = HumanoidModel::Part;
void SetSlotParts(HumanoidModel& model, ArmorSlot slot) {
    for (Part p : {Part::Head, Part::Body, Part::RightArm, Part::LeftArm, Part::RightLeg,
                   Part::LeftLeg}) {
        model.SetVisible(p, false);
    }
    switch (slot) {
    case ArmorSlot::Head:
        model.SetVisible(Part::Head, true);
        break;
    case ArmorSlot::Chest:
        model.SetVisible(Part::Body, true);
        model.SetVisible(Part::RightArm, true);
        model.SetVisible(Part::LeftArm, true);
        break;
    case ArmorSlot::Legs:
        model.SetVisible(Part::Body, true);
        model.SetVisible(Part::RightLeg, true);
        model.SetVisible(Part::LeftLeg, true);
        break;
    case ArmorSlot::Feet:
        model.SetVisible(Part::RightLeg, true);
        model.SetVisible(Part::LeftLeg, true);
        break;
    }
}
} // namespace

PlayerDoll::PlayerDoll() = default;
PlayerDoll::~PlayerDoll() = default;

PlayerDoll::ArmorModels& PlayerDoll::Models(const std::string& material) {
    ArmorModels& m = m_armor[material];
    if (!m.layer1) {
        const std::string base = "mc/textures/models/armor/" + material;
        // Layer textures are 64x32 and carry no head overlay (includeHat=false).
        m.layer1 = std::make_unique<HumanoidModel>(base + "_layer_1.png", false, 1.0f, 64.0f,
                                                   32.0f, false);
        m.layer2 = std::make_unique<HumanoidModel>(base + "_layer_2.png", false, 0.5f, 64.0f,
                                                   32.0f, false);
    }
    return m;
}

const std::shared_ptr<vox::Texture2D>* PlayerDoll::Bake(
    vox::Shader& shader, int wPx, int hPx, const std::array<ItemStack, kArmorSlots>& armor,
    float age) {
    if (!m_body.Ready() || wPx <= 0 || hPx <= 0) {
        return nullptr; // no skin overlay -> no doll (clean clone)
    }
    if (!m_fb || m_w != wPx || m_h != hPx) {
        m_fb = std::make_unique<vox::Framebuffer>(static_cast<uint32_t>(wPx),
                                                  static_cast<uint32_t>(hPx));
        m_w = wPx;
        m_h = hPx;
    }

    m_fb->Bind();
    vox::Renderer::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    vox::Renderer::Clear();
    vox::Renderer::SetCullFace(false); // both windings; depth sorts

    // y-up ortho in framebuffer pixels (matches DrawImage's stored-bottom-left
    // sampling, so the doll comes out upright — same trick as BlockIcons).
    const float w = static_cast<float>(wPx);
    const float h = static_cast<float>(hPx);
    const glm::mat4 proj = glm::ortho(0.0f, w, 0.0f, h, -1000.0f, 1000.0f);

    // The biped spans 32px (head top y=-8 .. feet y=24) in Y-down model space.
    // Scale to ~82% of the frame height; stand the feet a touch above the
    // bottom; flip Y-down -> the y-up frame; yaw for a 3/4 front view.
    const float scale = h * 0.82f / 32.0f;
    const float feetY = h * 0.10f;
    const float yaw = glm::radians(200.0f); // face the viewer, angled slightly
    glm::mat4 placement = glm::translate(glm::mat4(1.0f), {w * 0.5f, feetY, 0.0f});
    placement = glm::scale(placement, {scale, -scale, scale});
    placement = glm::rotate(placement, yaw, {0.0f, 1.0f, 0.0f});
    placement = glm::translate(placement, {0.0f, -24.0f, 0.0f});

    shader.Bind();
    shader.SetMat4("u_viewProj", proj);
    shader.SetInt("u_skin", 1);
    shader.SetFloat3("u_sunDir", glm::normalize(glm::vec3(0.35f, 0.9f, 0.55f)));
    shader.SetFloat("u_sunLight", 1.0f);
    shader.SetFloat3("u_skyTint", {1.0f, 1.0f, 1.0f});
    shader.SetFloat2("u_light", {1.0f, 1.0f}); // full bright, like the vanilla GUI
    shader.SetFloat("u_hurt", 0.0f);

    // Idle pose: no walk swing, a little age-driven arm sway.
    m_body.SetRotationAngles(0.0f, 0.0f, age, 0.0f, 0.0f);
    m_body.Render(shader, placement);

    // Worn armor layers, over the body (helmet/chest/boots from layer 1,
    // leggings from layer 2). Each piece shows only the parts it covers.
    for (int s = 0; s < kArmorSlots; ++s) {
        const ItemStack& piece = armor[static_cast<size_t>(s)];
        if (piece.Empty()) {
            continue;
        }
        const std::string& material = ArmorTexture(piece.id);
        if (material.empty()) {
            continue;
        }
        const auto slot = static_cast<ArmorSlot>(s);
        ArmorModels& models = Models(material);
        HumanoidModel* model = slot == ArmorSlot::Legs ? models.layer2.get() : models.layer1.get();
        if (!model->Ready()) {
            continue; // missing layer texture (clean clone) — skip silently
        }
        SetSlotParts(*model, slot);
        model->SetRotationAngles(0.0f, 0.0f, age, 0.0f, 0.0f);
        model->Render(shader, placement);
    }

    vox::Renderer::SetCullFace(true);
    vox::Framebuffer::Unbind();
    return &m_fb->Color();
}

} // namespace vc
