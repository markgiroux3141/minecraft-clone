#pragma once

#include <glm/glm.hpp>

#include "Inventory.h"
#include "ui/Widgets.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// Survival inventory screen (M17): the player's hotbar + 9x3 grid on
// vanilla's 176x166 container panel (gui/container/inventory.png), with a
// creative-style block palette above it — the only block source until
// M18's mining drops. Immediate-mode like the menus: GameApp owns the
// inventory, the carried stack, and the open/closed state.
class InventoryScreen {
public:
    // leftClick/rightClick are press edges. Slot clicks mutate inv and
    // carried (vanilla PICKUP rules: left = whole stack, right = half /
    // place one). Palette clicks load the carried stack for free; a click
    // outside both panels discards it (no item drops until M18).
    static void Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse, bool leftClick,
                     bool rightClick, Inventory& inv, ItemStack& carried,
                     const GuiTextures& tex);
};

} // namespace vc
