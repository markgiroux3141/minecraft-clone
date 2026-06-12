#include "Inventory.h"

#include <algorithm>

namespace vc {

ItemStack Inventory::Add(ItemStack stack) {
    if (stack.Empty()) {
        return {};
    }
    for (ItemStack& slot : m_slots) {
        if (slot.Empty() || slot.id != stack.id || slot.count >= kMaxStackSize) {
            continue;
        }
        const int moved = std::min(stack.count, kMaxStackSize - slot.count);
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
        slot = {stack.id, std::min(stack.count, kMaxStackSize)};
        stack.count -= slot.count;
        if (stack.count == 0) {
            return {};
        }
    }
    return stack; // inventory full
}

} // namespace vc
