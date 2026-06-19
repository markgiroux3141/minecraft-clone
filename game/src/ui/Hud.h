#pragma once

#include <span>

#include <glm/glm.hpp>

#include "item/Inventory.h"
#include "ui/Widgets.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// Minecraft's auto GUI scale: the largest integer scale that keeps a 320x240
// layout on screen (3 at 1600x900, 4 at 4k). Shared by all overlay layouts.
float GuiScale(glm::vec2 screen);

// M30 player vitals shown above the hotbar (hearts + food, plus air bubbles
// while submerged). `show` is false in creative/fly mode (vanilla hides them).
struct HudVitals {
    float health = 20.0f; // 0..20, 2 = one heart
    int food = 20;        // 0..20, 2 = one drumstick
    int air = 300;        // 0..300; bubbles only drawn when below 300
    bool show = false;
};

// In-game overlay: crosshair + hotbar + vitals + a debug coordinate readout.
// Stateless — layout is recomputed from the screen size every frame, scaled in
// integer steps (Minecraft GUI scale). `playerPos` is the player's feet-center
// world position; the HUD shows its floored block coords top-left.
class Hud {
public:
    static void Draw(vox::UiRenderer& ui, glm::vec2 screen, std::span<const ItemStack> hotbar,
                     size_t selectedSlot, const GuiTextures& tex, const HudVitals& vitals,
                     const glm::vec3& playerPos);
};

} // namespace vc
