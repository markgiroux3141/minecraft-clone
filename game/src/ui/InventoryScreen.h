#pragma once

#include <span>

#include <glm/glm.hpp>

#include "Inventory.h"
#include "ui/Widgets.h"
#include "world/Furnace.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// Container screens (M17/M19): the player inventory (vanilla 176x166
// panel + 2x2 craft grid + creative block/item palette) and the crafting
// table (3x3 grid, no palette). Immediate-mode like the menus: GameApp
// owns the inventory, the craft grid, the carried stack, and the state.
class InventoryScreen {
public:
    // craft is row-major craftSize x craftSize (2 = player grid, 3 =
    // crafting table; selects the panel art too). leftClick/rightClick
    // are press edges. Slot clicks follow vanilla PICKUP rules; the
    // result slot crafts into the cursor and consumes one from each grid
    // cell. A click outside the panels moves the carried stack into
    // `thrown` — the caller tosses it into the world.
    static void Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse, bool leftClick,
                     bool rightClick, Inventory& inv, std::span<ItemStack> craft, int craftSize,
                     ItemStack& carried, ItemStack& thrown, const GuiTextures& tex);

    // The furnace screen (M21): vanilla ContainerFurnace layout — input
    // over fuel on the left, take-only output on the right, the flame and
    // arrow overlays sized by the live FurnaceState (the world keeps
    // ticking it while the screen is open).
    static void DrawFurnace(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse,
                            bool leftClick, bool rightClick, Inventory& inv,
                            FurnaceState& furnace, ItemStack& carried, ItemStack& thrown,
                            const GuiTextures& tex);
};

} // namespace vc
