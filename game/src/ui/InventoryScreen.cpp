#include "ui/InventoryScreen.h"

#include <algorithm>
#include <string>
#include <vector>

#include "vox/renderer/UiRenderer.h"

#include "Crafting.h"
#include "ui/Hud.h" // GuiScale

namespace vc {

namespace {

// Container art layouts, straight from the vanilla containers:
// ContainerPlayer's 2x2 at (98, 18) with the result at (154, 28);
// ContainerWorkbench's 3x3 at (30, 17) with the result at (124, 35).
// Both share the main grid at (8, 84) and the hotbar row at (8, 142),
// 16px slot interiors on an 18px pitch.
constexpr glm::vec2 kPanelSize{176.0f, 166.0f};
constexpr float kSlotPitch = 18.0f;
constexpr float kSlotsX = 8.0f;
constexpr float kGridY = 84.0f;
constexpr float kHotbarY = 142.0f;
constexpr glm::vec2 kPlayerCraftPos{98.0f, 18.0f};
constexpr glm::vec2 kPlayerResultPos{154.0f, 28.0f};
constexpr glm::vec2 kTableCraftPos{30.0f, 17.0f};
constexpr glm::vec2 kTableResultPos{124.0f, 35.0f};
constexpr float kPanelGap = 4.0f; // between the palette and inventory panels

constexpr int kPaletteColumns = 9;
constexpr float kPaletteTitleBand = 17.0f; // title row above the entries
constexpr float kPaletteBottomPad = 7.0f;

constexpr glm::vec4 kPanelFill{0.776f, 0.776f, 0.776f, 1.0f}; // vanilla 0xC6C6C6
constexpr glm::vec4 kPanelFrame{0.12f, 0.12f, 0.12f, 0.95f};
constexpr glm::vec4 kSlotFill{0.545f, 0.545f, 0.545f, 1.0f}; // vanilla 0x8B8B8B
constexpr glm::vec4 kTitleColor{0.25f, 0.25f, 0.25f, 1.0f};  // vanilla 0x404040
constexpr glm::vec4 kHoverHighlight{1.0f, 1.0f, 1.0f, 0.5f};
constexpr glm::vec4 kTooltipFill{0.06f, 0.03f, 0.11f, 0.94f};

// Image-local position of an inventory slot's 16x16 interior.
glm::vec2 SlotPos(size_t index) {
    if (index < Inventory::kHotbarSize) {
        return {kSlotsX + static_cast<float>(index) * kSlotPitch, kHotbarY};
    }
    const size_t i = index - Inventory::kHotbarSize;
    return {kSlotsX + static_cast<float>(i % 9) * kSlotPitch,
            kGridY + static_cast<float>(i / 9) * kSlotPitch};
}

bool Hover(glm::vec2 mouse, glm::vec2 pos, glm::vec2 size) {
    return mouse.x >= pos.x && mouse.x < pos.x + size.x && mouse.y >= pos.y &&
           mouse.y < pos.y + size.y;
}

bool SameKind(const ItemStack& a, const ItemStack& b) {
    return a.id == b.id && a.damage == b.damage;
}

// Vanilla PICKUP rules. Left: pick up / place all / merge / swap. Right:
// pick up the larger half / place one / swap on mismatch. Stacks merge
// only when id AND damage match, capped at the item's own max.
void ClickSlot(ItemStack& slot, ItemStack& carried, bool right) {
    if (!right) {
        if (!carried.Empty() && SameKind(carried, slot)) {
            const int moved =
                std::min(carried.count, ItemMaxStack(slot.id) - slot.count);
            slot.count += std::max(moved, 0);
            carried.count -= std::max(moved, 0);
            if (carried.count == 0) {
                carried = {};
            }
        } else {
            std::swap(slot, carried);
        }
        return;
    }
    if (carried.Empty()) {
        if (!slot.Empty()) {
            carried = {slot.id, (slot.count + 1) / 2, slot.damage};
            slot.count -= carried.count;
            if (slot.count == 0) {
                slot = {};
            }
        }
    } else if (slot.Empty() ||
               (SameKind(slot, carried) && slot.count < ItemMaxStack(slot.id))) {
        if (slot.Empty()) {
            slot = {carried.id, 0, carried.damage};
        }
        ++slot.count;
        if (--carried.count == 0) {
            carried = {};
        }
    } else if (!SameKind(slot, carried)) {
        std::swap(slot, carried);
    }
}

// Every obtainable thing: blocks (minus air and the internal flowing-water
// levels) followed by the registry items (sticks, tools).
std::vector<ItemId> PaletteIds() {
    std::vector<ItemId> ids;
    const auto& blockRegistry = BlockRegistry::Get();
    for (BlockId id = 1; id < static_cast<BlockId>(blockRegistry.Count()); ++id) {
        const uint8_t level = blockRegistry.Def(id).liquidLevel;
        if (level == 0 || level == 8) {
            ids.push_back(id);
        }
    }
    for (size_t i = 0; i < ItemRegistry::Get().Count(); ++i) {
        ids.push_back(static_cast<ItemId>(kFirstItemId + i));
    }
    return ids;
}

} // namespace

void InventoryScreen::Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse, bool leftClick,
                           bool rightClick, Inventory& inv, std::span<ItemStack> craft,
                           int craftSize, ItemStack& carried, ItemStack& thrown,
                           const GuiTextures& tex) {
    const float s = GuiScale(screen);
    const bool table = craftSize == 3;
    const bool click = leftClick || rightClick;
    std::string tooltip;

    // The player screen carries the creative palette above the panel; the
    // crafting table is just the panel. Center the stack either way.
    const std::vector<ItemId> palette = table ? std::vector<ItemId>{} : PaletteIds();
    const auto paletteRows = static_cast<float>(
        (palette.size() + kPaletteColumns - 1) / kPaletteColumns);
    const glm::vec2 paletteSize{
        kPanelSize.x, table ? 0.0f
                            : kPaletteTitleBand + paletteRows * kSlotPitch + kPaletteBottomPad};
    const float aboveHeight = table ? 0.0f : (paletteSize.y + kPanelGap);
    const float totalHeight = (aboveHeight + kPanelSize.y) * s;
    const glm::vec2 paletteOrigin{std::floor((screen.x - kPanelSize.x * s) * 0.5f),
                                  std::floor((screen.y - totalHeight) * 0.5f)};
    const glm::vec2 panelOrigin = paletteOrigin + glm::vec2(0.0f, aboveHeight * s);

    if (!table) {
        // Palette panel (always procedural — there is no vanilla art for it).
        ui.DrawRect(paletteOrigin - glm::vec2(s), paletteSize * s + glm::vec2(2.0f * s),
                    kPanelFrame);
        ui.DrawRect(paletteOrigin, paletteSize * s, kPanelFill);
        ui.DrawText(paletteOrigin + glm::vec2(8.0f * s, 6.0f * s), "Blocks & items",
                    UiTextScale(ui, s), kTitleColor);
        for (size_t i = 0; i < palette.size(); ++i) {
            const glm::vec2 pos =
                paletteOrigin +
                glm::vec2(kSlotsX + static_cast<float>(i % kPaletteColumns) * kSlotPitch,
                          kPaletteTitleBand +
                              static_cast<float>(i / kPaletteColumns) * kSlotPitch) *
                    s;
            ui.DrawRect(pos, glm::vec2(16.0f * s), kSlotFill);
            DrawItemStack(ui, pos, s, {palette[i], 1});
            if (Hover(mouse, pos, glm::vec2(16.0f * s))) {
                ui.DrawRect(pos, glm::vec2(16.0f * s), kHoverHighlight);
                tooltip = ItemName(palette[i]);
                // The palette is the free creative source: left grabs a
                // full stack (replacing whatever was carried), right adds
                // one.
                const int grab = std::min(kMaxStackSize, ItemMaxStack(palette[i]));
                if (leftClick) {
                    carried = {palette[i], grab};
                } else if (rightClick) {
                    if (carried.id == palette[i] && !carried.Empty()) {
                        carried.count = std::min(carried.count + 1, grab);
                    } else {
                        carried = {palette[i], 1};
                    }
                }
            }
        }
    }

    // Container panel.
    const auto& panelTex = table ? tex.craftingTable : tex.inventory;
    const glm::vec2 craftPos = table ? kTableCraftPos : kPlayerCraftPos;
    const glm::vec2 resultPos = table ? kTableResultPos : kPlayerResultPos;
    if (panelTex) {
        ui.DrawImage(panelTex, panelOrigin, kPanelSize * s, {0.0f, 0.0f}, kPanelSize);
    } else {
        ui.DrawRect(panelOrigin - glm::vec2(s), kPanelSize * s + glm::vec2(2.0f * s),
                    kPanelFrame);
        ui.DrawRect(panelOrigin, kPanelSize * s, kPanelFill);
        for (size_t i = 0; i < Inventory::kSize; ++i) {
            ui.DrawRect(panelOrigin + SlotPos(i) * s, glm::vec2(16.0f * s), kSlotFill);
        }
        for (int i = 0; i < craftSize * craftSize; ++i) {
            const glm::vec2 pos =
                craftPos + glm::vec2(static_cast<float>(i % craftSize),
                                     static_cast<float>(i / craftSize)) *
                               kSlotPitch;
            ui.DrawRect(panelOrigin + pos * s, glm::vec2(16.0f * s), kSlotFill);
        }
        ui.DrawRect(panelOrigin + resultPos * s, glm::vec2(16.0f * s), kSlotFill);
    }

    // A regular interactive slot (inventory and craft cells alike).
    const auto doSlot = [&](ItemStack& stack, glm::vec2 pos) {
        DrawItemStack(ui, pos, s, stack);
        if (Hover(mouse, pos, glm::vec2(16.0f * s))) {
            ui.DrawRect(pos, glm::vec2(16.0f * s), kHoverHighlight);
            if (!stack.Empty()) {
                tooltip = ItemName(stack.id);
            }
            if (click) {
                ClickSlot(stack, carried, rightClick);
            }
        }
    };
    for (size_t i = 0; i < Inventory::kSize; ++i) {
        doSlot(inv.Slot(i), panelOrigin + SlotPos(i) * s);
    }
    for (int i = 0; i < craftSize * craftSize; ++i) {
        const glm::vec2 pos = craftPos + glm::vec2(static_cast<float>(i % craftSize),
                                                   static_cast<float>(i / craftSize)) *
                                             kSlotPitch;
        doSlot(craft[static_cast<size_t>(i)], panelOrigin + pos * s);
    }

    // Result slot: shows the recipe match; clicking crafts into the
    // cursor (vanilla: only when it fits) and consumes one per grid cell.
    const ItemStack result = Recipes::Match(craft, craftSize);
    const glm::vec2 resultDraw = panelOrigin + resultPos * s;
    DrawItemStack(ui, resultDraw, s, result);
    if (Hover(mouse, resultDraw, glm::vec2(16.0f * s))) {
        ui.DrawRect(resultDraw, glm::vec2(16.0f * s), kHoverHighlight);
        if (!result.Empty()) {
            tooltip = ItemName(result.id);
        }
        if (click && !result.Empty()) {
            const bool fits =
                carried.Empty() || (SameKind(carried, result) &&
                                    carried.count + result.count <= ItemMaxStack(result.id));
            if (fits) {
                if (carried.Empty()) {
                    carried = result;
                } else {
                    carried.count += result.count;
                }
                for (ItemStack& cell : craft) {
                    if (!cell.Empty() && --cell.count <= 0) {
                        cell = {};
                    }
                }
            }
        }
    }

    // A click outside the panels throws the carried stack (vanilla): the
    // caller spawns it as an item entity in front of the player.
    if (click && !carried.Empty() &&
        (table || !Hover(mouse, paletteOrigin, paletteSize * s)) &&
        !Hover(mouse, panelOrigin, kPanelSize * s)) {
        thrown = carried;
        carried = {};
    }

    if (!carried.Empty()) {
        DrawItemStack(ui, mouse - glm::vec2(8.0f * s), s, carried);
    } else if (!tooltip.empty()) {
        const float textScale = UiTextScale(ui, s);
        const glm::vec2 size = ui.MeasureText(tooltip, textScale);
        const glm::vec2 pos = mouse + glm::vec2(12.0f * s, -12.0f * s);
        ui.DrawRect(pos - glm::vec2(3.0f * s), size + glm::vec2(6.0f * s), kTooltipFill);
        ShadowedText(ui, pos, tooltip, textScale);
    }
}

} // namespace vc
