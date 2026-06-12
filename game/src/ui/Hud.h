#pragma once

#include <span>

#include <glm/glm.hpp>

#include "Inventory.h"
#include "ui/Widgets.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// Minecraft's auto GUI scale: the largest integer scale that keeps a 320x240
// layout on screen (3 at 1600x900, 4 at 4k). Shared by all overlay layouts.
float GuiScale(glm::vec2 screen);

// In-game overlay: crosshair + hotbar. Stateless — layout is recomputed from
// the screen size every frame, scaled in integer steps (Minecraft GUI scale).
class Hud {
public:
    static void Draw(vox::UiRenderer& ui, glm::vec2 screen, std::span<const ItemStack> hotbar,
                     size_t selectedSlot, const GuiTextures& tex);
};

} // namespace vc
