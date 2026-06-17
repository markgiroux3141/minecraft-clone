#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "entity/HumanoidModel.h"
#include "item/Inventory.h"

namespace vox {
class Framebuffer;
class Shader;
class Texture2D;
} // namespace vox

namespace vc {

// M33: renders the player character (Steve body + worn armor layers) to an
// offscreen texture for the inventory screen, reusing M29's framebuffer->UI
// path and M31's box-model renderer. The armor layers are inflated bipeds
// (vanilla LayerArmorBase: layer 1 at +1.0 for helmet/chest/boots, layer 2 at
// +0.5 for leggings) skinned with the {material}_layer_{1,2}.png textures.
class PlayerDoll {
public:
    PlayerDoll();
    ~PlayerDoll(); // out-of-line: m_fb's unique_ptr<Framebuffer> needs the full type

    // Bakes the body + worn armor into a wPx x hPx offscreen texture (the
    // framebuffer is recreated only when the size changes), lit flat like the
    // vanilla GUI. `armor` is the four worn slots (Head/Chest/Legs/Feet);
    // empty slots draw nothing. `age` drives the idle sway. Returns the baked
    // color texture, or null when the body skin overlay is absent (a clean
    // clone draws no doll, like the debug Steve). The caller must restore its
    // own viewport afterward (Bind set the viewport to the framebuffer size).
    const std::shared_ptr<vox::Texture2D>* Bake(vox::Shader& shader, int wPx, int hPx,
                                                 const std::array<ItemStack, kArmorSlots>& armor,
                                                 float age);

private:
    // The two armor models for one material: layer 1 (helmet/chest/boots) and
    // layer 2 (leggings), built lazily on first use.
    struct ArmorModels {
        std::unique_ptr<HumanoidModel> layer1; // inflate 1.0, {mat}_layer_1.png
        std::unique_ptr<HumanoidModel> layer2; // inflate 0.5, {mat}_layer_2.png
    };
    ArmorModels& Models(const std::string& material);

    HumanoidModel m_body;
    std::unordered_map<std::string, ArmorModels> m_armor;
    std::unique_ptr<vox::Framebuffer> m_fb;
    int m_w = 0;
    int m_h = 0;
};

} // namespace vc
