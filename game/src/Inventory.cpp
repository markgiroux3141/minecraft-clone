#include "Inventory.h"

#include <algorithm>

namespace vc {

ItemStack Inventory::Add(ItemStack stack) {
    if (stack.Empty()) {
        return {};
    }
    const int maxStack = ItemMaxStack(stack.id);
    for (ItemStack& slot : m_slots) {
        if (slot.Empty() || slot.id != stack.id || slot.damage != stack.damage ||
            slot.count >= maxStack) {
            continue;
        }
        const int moved = std::min(stack.count, maxStack - slot.count);
        slot.count += moved;
        stack.count -= moved;
        if (stack.count == 0) {
            return {};
        }
    }
    for (ItemStack& slot : m_slots) {
        if (!slot.Empty()) {
            continue;
        }
        slot = {stack.id, std::min(stack.count, maxStack), stack.damage};
        stack.count -= slot.count;
        if (stack.count == 0) {
            return {};
        }
    }
    return stack; // inventory full
}

} // namespace vc
