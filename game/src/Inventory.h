#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "world/Block.h"

namespace vc {

// A stack of one block type. Blocks are the only item kind until the
// survival milestones grow a separate item registry (tools, M19).
struct ItemStack {
    BlockId id = blocks::Air;
    int count = 0;

    bool Empty() const { return id == blocks::Air || count <= 0; }
};

inline constexpr int kMaxStackSize = 64;

// The player's slots, vanilla InventoryPlayer layout: 0..8 are the hotbar,
// 9..35 the main 9x3 grid. An empty slot is the empty hand.
class Inventory {
public:
    static constexpr size_t kHotbarSize = 9;
    static constexpr size_t kSize = 36;

    ItemStack& Slot(size_t index) { return m_slots[index]; }
    const ItemStack& Slot(size_t index) const { return m_slots[index]; }
    std::span<const ItemStack> Hotbar() const { return {m_slots.data(), kHotbarSize}; }

    // Merges into matching stacks first, then the first empty slot (slot
    // order, so the hotbar fills before the grid — vanilla's rule).
    // Returns whatever didn't fit.
    ItemStack Add(ItemStack stack);

    void Clear() { m_slots.fill(ItemStack{}); }

private:
    std::array<ItemStack, kSize> m_slots{};
};

} // namespace vc
