#include "world/Furnace.h"

#include <algorithm>

namespace vc::furnace {

ItemStack SmeltResult(ItemId id) {
    if (id == blocks::IronOre) {
        return {items::IronIngot, 1};
    }
    if (id == blocks::Sand) {
        return {blocks::Glass, 1};
    }
    if (id == blocks::Cobblestone) {
        return {blocks::Stone, 1};
    }
    return {};
}

int BurnTime(ItemId id) {
    if (id == items::Coal) {
        return 1600;
    }
    if (id == blocks::Planks || id == blocks::CraftingTable || id == blocks::Log ||
        id == blocks::BirchLog || id == blocks::SpruceLog) {
        return 300;
    }
    if (id == items::WoodPickaxe || id == items::WoodAxe || id == items::WoodShovel) {
        return 200;
    }
    if (id == items::Stick) {
        return 100;
    }
    return 0;
}

namespace {

// Has an input with a recipe, and room for the result in the output slot.
bool CanSmelt(const FurnaceState& st) {
    if (st.input.Empty()) {
        return false;
    }
    const ItemStack result = SmeltResult(st.input.id);
    if (result.Empty()) {
        return false;
    }
    if (st.output.Empty()) {
        return true;
    }
    return st.output.id == result.id && st.output.damage == result.damage &&
           st.output.count + result.count <= ItemMaxStack(result.id);
}

void Smelt(FurnaceState& st) {
    const ItemStack result = SmeltResult(st.input.id);
    if (st.output.Empty()) {
        st.output = result;
    } else {
        st.output.count += result.count;
    }
    if (--st.input.count <= 0) {
        st.input = {};
    }
}

} // namespace

bool Tick(FurnaceState& st) {
    // TileEntityFurnace.update, minus the world plumbing.
    if (st.burnTicks > 0) {
        --st.burnTicks;
    }

    if (st.burnTicks > 0 || (!st.fuel.Empty() && !st.input.Empty())) {
        if (st.burnTicks <= 0 && CanSmelt(st)) {
            st.burnTicks = st.burnTotal = BurnTime(st.fuel.id);
            if (st.burnTicks > 0) {
                if (--st.fuel.count <= 0) {
                    st.fuel = {};
                }
            }
        }
        if (st.burnTicks > 0 && CanSmelt(st)) {
            if (++st.cookTicks >= kCookTicks) {
                st.cookTicks = 0;
                Smelt(st);
            }
        } else {
            st.cookTicks = 0;
        }
    } else if (st.cookTicks > 0) {
        // Unlit with partial progress: decay it (vanilla: -2/tick).
        st.cookTicks = std::clamp(st.cookTicks - 2, 0, kCookTicks);
    }

    return st.burnTicks > 0;
}

} // namespace vc::furnace
