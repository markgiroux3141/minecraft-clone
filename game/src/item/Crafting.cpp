#include "item/Crafting.h"

#include <algorithm>

#include "vox/core/Assert.h"
#include "vox/core/Log.h"

namespace vc {

namespace {

std::vector<Recipe> g_recipes;

bool Accepts(const std::vector<ItemId>& options, const ItemStack& stack) {
    if (options.empty()) {
        return stack.Empty();
    }
    return !stack.Empty() && std::find(options.begin(), options.end(), stack.id) != options.end();
}

// Does the recipe pattern, anchored at (ox, oy) in the grid (optionally
// mirrored), match exactly — including every cell OUTSIDE the pattern
// being empty?
bool MatchShapedAt(const Recipe& r, std::span<const ItemStack> grid, int gridSize, int ox, int oy,
                   bool mirrored) {
    for (int gy = 0; gy < gridSize; ++gy) {
        for (int gx = 0; gx < gridSize; ++gx) {
            const ItemStack& stack = grid[static_cast<size_t>(gy * gridSize + gx)];
            const int rx = mirrored ? r.width - 1 - (gx - ox) : gx - ox;
            const int ry = gy - oy;
            if (rx < 0 || rx >= r.width || ry < 0 || ry >= r.height) {
                if (!stack.Empty()) {
                    return false;
                }
                continue;
            }
            if (!Accepts(r.cells[static_cast<size_t>(ry * r.width + rx)], stack)) {
                return false;
            }
        }
    }
    return true;
}

bool MatchShaped(const Recipe& r, std::span<const ItemStack> grid, int gridSize) {
    if (r.width > gridSize || r.height > gridSize) {
        return false;
    }
    for (int oy = 0; oy + r.height <= gridSize; ++oy) {
        for (int ox = 0; ox + r.width <= gridSize; ++ox) {
            if (MatchShapedAt(r, grid, gridSize, ox, oy, false) ||
                MatchShapedAt(r, grid, gridSize, ox, oy, true)) {
                return true;
            }
        }
    }
    return false;
}

bool MatchShapeless(const Recipe& r, std::span<const ItemStack> grid) {
    std::vector<const ItemStack*> present;
    for (const ItemStack& stack : grid) {
        if (!stack.Empty()) {
            present.push_back(&stack);
        }
    }
    if (present.size() != r.ingredients.size()) {
        return false;
    }
    // Greedy bipartite match — fine at these sizes (vanilla does the same).
    std::vector<bool> used(present.size(), false);
    for (const auto& options : r.ingredients) {
        bool found = false;
        for (size_t i = 0; i < present.size(); ++i) {
            if (!used[i] && Accepts(options, *present[i])) {
                used[i] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// Pattern rows as strings; key maps a char to acceptable ids, ' ' = empty.
Recipe Shaped(std::vector<std::string> rows,
              std::vector<std::pair<char, std::vector<ItemId>>> key, ItemStack result) {
    Recipe r;
    r.width = static_cast<int>(rows[0].size());
    r.height = static_cast<int>(rows.size());
    for (const std::string& row : rows) {
        for (const char c : row) {
            if (c == ' ') {
                r.cells.emplace_back();
                continue;
            }
            const auto it = std::find_if(key.begin(), key.end(),
                                         [&](const auto& kv) { return kv.first == c; });
            VOX_ASSERT(it != key.end(), "recipe key missing a pattern char");
            r.cells.push_back(it->second);
        }
    }
    r.result = result;
    return r;
}

} // namespace

void Recipes::RegisterDefaults() {
    if (!g_recipes.empty()) {
        return;
    }
    const std::vector<ItemId> anyLog{blocks::Log, blocks::BirchLog, blocks::SpruceLog};
    const std::vector<ItemId> planks{blocks::Planks};
    const std::vector<ItemId> stick{items::Stick};
    const std::vector<ItemId> cobble{blocks::Cobblestone};

    // Any log -> 4 planks (shapeless single ingredient).
    Recipe logToPlanks;
    logToPlanks.shapeless = true;
    logToPlanks.ingredients = {anyLog};
    logToPlanks.result = {blocks::Planks, 4};
    g_recipes.push_back(std::move(logToPlanks));

    // Two planks stacked -> 4 sticks.
    g_recipes.push_back(Shaped({"P", "P"}, {{'P', planks}}, {items::Stick, 4}));
    // 2x2 planks -> crafting table.
    g_recipes.push_back(Shaped({"PP", "PP"}, {{'P', planks}}, {blocks::CraftingTable, 1}));
    // 2x2 sand -> sandstone (vanilla).
    g_recipes.push_back(
        Shaped({"SS", "SS"}, {{'S', {blocks::Sand}}}, {blocks::Sandstone, 1}));

    // Tools: M = material head, S = stick handle. Axe patterns mirror
    // automatically (MatchShaped tries both orientations).
    const auto tools = [&](const std::vector<ItemId>& m, ItemId pickaxe, ItemId axe,
                           ItemId shovel) {
        g_recipes.push_back(
            Shaped({"MMM", " S ", " S "}, {{'M', m}, {'S', stick}}, {pickaxe, 1}));
        g_recipes.push_back(Shaped({"MM", "MS", " S"}, {{'M', m}, {'S', stick}}, {axe, 1}));
        g_recipes.push_back(Shaped({"M", "S", "S"}, {{'M', m}, {'S', stick}}, {shovel, 1}));
    };
    tools(planks, items::WoodPickaxe, items::WoodAxe, items::WoodShovel);
    tools(cobble, items::StonePickaxe, items::StoneAxe, items::StoneShovel);
    tools({items::IronIngot}, items::IronPickaxe, items::IronAxe, items::IronShovel);

    // M21: 8 cobblestone in a ring -> furnace (3x3 grid only).
    g_recipes.push_back(
        Shaped({"CCC", "C C", "CCC"}, {{'C', cobble}}, {blocks::Furnace, 1}));
    // Coal over a stick -> 4 torches (vanilla).
    g_recipes.push_back(
        Shaped({"C", "S"}, {{'C', {items::Coal}}, {'S', stick}}, {blocks::Torch, 4}));
    // M26: three iron ingots in a V -> a bucket (vanilla).
    g_recipes.push_back(
        Shaped({"I I", " I "}, {{'I', {items::IronIngot}}}, {items::Bucket, 1}));

    // M28: per material, 3-in-a-row -> 6 slabs; the stair triangle -> 4 stairs
    // (MatchShaped tries the mirror, so both orientations craft).
    const auto shapeRecipes = [&](ItemId base, ItemId slab, ItemId stairs) {
        g_recipes.push_back(Shaped({"MMM"}, {{'M', {base}}}, {slab, 6}));
        g_recipes.push_back(
            Shaped({"M  ", "MM ", "MMM"}, {{'M', {base}}}, {stairs, 4}));
    };
    shapeRecipes(blocks::Stone, blocks::StoneSlab, blocks::StoneStairs);
    shapeRecipes(blocks::Cobblestone, blocks::CobbleSlab, blocks::CobbleStairs);
    shapeRecipes(blocks::Planks, blocks::PlankSlab, blocks::PlankStairs);
    shapeRecipes(blocks::Sandstone, blocks::SandstoneSlab, blocks::SandstoneStairs);

    GAME_INFO("Registered {} crafting recipes", g_recipes.size());
}

ItemStack Recipes::Match(std::span<const ItemStack> grid, int gridSize) {
    for (const Recipe& r : g_recipes) {
        if (r.shapeless ? MatchShapeless(r, grid) : MatchShaped(r, grid, gridSize)) {
            return r.result;
        }
    }
    return {};
}

} // namespace vc
