#pragma once

#include "item/Inventory.h" // ItemStack

namespace vc {

// Per-furnace block-entity state (M21): the first block with state beyond
// its id. Lives in World's position-keyed furnace map (created lazily on
// first use, spilled as item drops when the block breaks) and persists in
// the save dir's furnaces.dat sidecar. The block id itself only tracks
// lit/unlit (separate appended ids, swapped like the water flow levels).
struct FurnaceState {
    ItemStack input;  // what's being smelted
    ItemStack fuel;   // what's burning next
    ItemStack output; // take-only result slot
    int burnTicks = 0;  // burn time left on the current fuel
    int burnTotal = 0;  // what burnTicks started at (flame fraction)
    int cookTicks = 0;  // 0..kCookTicks progress on the current input
};

namespace furnace {

// Vanilla TileEntityFurnace: every smelt takes 200 ticks (10 s).
inline constexpr int kCookTicks = 200;

// The smelting result for one input item, or an empty stack when the item
// doesn't smelt. (FurnaceRecipes analog, sized for our item set.)
ItemStack SmeltResult(ItemId id);

// Burn duration in ticks, 0 when the item isn't fuel (getItemBurnTime:
// coal 1600, wood blocks 300, wooden tools 200, stick 100).
int BurnTime(ItemId id);

// One 20-TPS tick of the vanilla furnace rules: consume fuel when a smelt
// can start, advance/reset cook progress, move results to the output
// slot. Returns true when the furnace is burning after the tick (the
// caller swaps the lit/unlit block to match).
bool Tick(FurnaceState& state);

} // namespace furnace

} // namespace vc
