#pragma once

#include <span>
#include <vector>

#include "Inventory.h"

namespace vc {

// Crafting recipes (M19). Shaped recipes match their pattern anywhere in
// the grid, original or horizontally mirrored (vanilla's rule); shapeless
// recipes match any arrangement. Each ingredient is a list of acceptable
// ids ("any log").
struct Recipe {
    bool shapeless = false;
    int width = 0, height = 0;                    // shaped only
    std::vector<std::vector<ItemId>> cells;       // shaped: width*height, {} = empty
    std::vector<std::vector<ItemId>> ingredients; // shapeless
    ItemStack result;
};

class Recipes {
public:
    // Registers the starter set (logs->planks, sticks, table, sandstone,
    // wood/stone pickaxe/axe/shovel). Call after item registration.
    static void RegisterDefaults();

    // grid is row-major gridSize x gridSize (2 = inventory, 3 = table).
    // Returns the crafted result, or an empty stack when nothing matches.
    static ItemStack Match(std::span<const ItemStack> grid, int gridSize);
};

} // namespace vc
