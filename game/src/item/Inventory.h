#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "item/Item.h"

namespace vc {

// A stack of one item kind (block or registry item, see Item.h). damage
// is tool wear (0 for everything else); stacks only merge when both id
// and damage match, and tools stack to 1 anyway.
struct ItemStack {
    ItemId id = blocks::Air;
    int count = 0;
    int damage = 0;

    bool Empty() const { return id == blocks::Air || count <= 0; }
};

// Global ceiling (per-item limits via ItemMaxStack can only be lower).
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

    // M33 worn armor, indexed by ArmorSlot (Head/Chest/Legs/Feet). Separate
    // from the 36 main slots; persisted on its own manifest line.
    ItemStack& Armor(ArmorSlot slot) { return m_armor[static_cast<size_t>(slot)]; }
    const ItemStack& Armor(ArmorSlot slot) const { return m_armor[static_cast<size_t>(slot)]; }
    ItemStack& Armor(size_t index) { return m_armor[index]; }
    const ItemStack& Armor(size_t index) const { return m_armor[index]; }
    const std::array<ItemStack, kArmorSlots>& ArmorSlots() const { return m_armor; }

    // Merges into matching stacks first, then the first empty slot (slot
    // order, so the hotbar fills before the grid — vanilla's rule).
    // Returns whatever didn't fit.
    ItemStack Add(ItemStack stack);

    void Clear() {
        m_slots.fill(ItemStack{});
        m_armor.fill(ItemStack{});
    }

private:
    std::array<ItemStack, kSize> m_slots{};
    std::array<ItemStack, kArmorSlots> m_armor{};
};

} // namespace vc
