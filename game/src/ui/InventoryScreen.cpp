#include "ui/InventoryScreen.h"

#include <algorithm>
#include <string>
#include <vector>

#include "vox/core/Log.h"
#include "vox/renderer/UiRenderer.h"

#include "ui/Hud.h" // GuiScale

namespace vc {

namespace {

// gui/container/inventory.png layout (ContainerPlayer's slot grid): main
// 9x3 at (8 + c*18, 84 + r*18), hotbar row at (8 + c*18, 142), 16px slot
// interiors on an 18px pitch. The armor/crafting regions stay inert art
// until their milestones (M19 crafting).
constexpr glm::vec2 kPanelSize{176.0f, 166.0f};
constexpr float kSlotPitch = 18.0f;
constexpr float kSlotsX = 8.0f;
constexpr float kGridY = 84.0f;
constexpr float kHotbarY = 142.0f;
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

// Vanilla PICKUP rules. Left: pick up / place all / merge / swap. Right:
// pick up the larger half / place one / swap on mismatch.
void ClickSlot(ItemStack& slot, ItemStack& carried, bool right) {
    if (!right) {
        if (!carried.Empty() && carried.id == slot.id) {
            const int moved = std::min(carried.count, kMaxStackSize - slot.count);
            slot.count += moved;
            carried.count -= moved;
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
            carried = {slot.id, (slot.count + 1) / 2};
            slot.count -= carried.count;
            if (slot.count == 0) {
                slot = {};
            }
        }
    } else if (slot.Empty() || (slot.id == carried.id && slot.count < kMaxStackSize)) {
        if (slot.Empty()) {
            slot = {carried.id, 0};
        }
        ++slot.count;
        if (--carried.count == 0) {
            carried = {};
        }
    } else if (slot.id != carried.id) {
        std::swap(slot, carried);
    }
}

// Every placeable block: all registrations except air and the internal
// flowing-water levels (the source block stands in for water).
std::vector<BlockId> PaletteIds() {
    std::vector<BlockId> ids;
    const auto& registry = BlockRegistry::Get();
    for (BlockId id = 1; id < static_cast<BlockId>(registry.Count()); ++id) {
        const uint8_t level = registry.Def(id).liquidLevel;
        if (level == 0 || level == 8) {
            ids.push_back(id);
        }
    }
    return ids;
}

} // namespace

void InventoryScreen::Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse, bool leftClick,
                           bool rightClick, Inventory& inv, ItemStack& carried,
                           const GuiTextures& tex) {
    const float s = GuiScale(screen);
    const std::vector<BlockId> palette = PaletteIds();
    const auto paletteRows =
        static_cast<float>((palette.size() + kPaletteColumns - 1) / kPaletteColumns);
    const glm::vec2 paletteSize{kPanelSize.x,
                                kPaletteTitleBand + paletteRows * kSlotPitch + kPaletteBottomPad};

    // The palette sits above the inventory panel; center the pair.
    const float totalHeight = (paletteSize.y + kPanelGap + kPanelSize.y) * s;
    const glm::vec2 paletteOrigin{std::floor((screen.x - kPanelSize.x * s) * 0.5f),
                                  std::floor((screen.y - totalHeight) * 0.5f)};
    const glm::vec2 panelOrigin = paletteOrigin + glm::vec2(0.0f, (paletteSize.y + kPanelGap) * s);

    const bool click = leftClick || rightClick;
    std::string tooltip;

    // Palette panel (always procedural — there is no vanilla art for it).
    ui.DrawRect(paletteOrigin - glm::vec2(s), paletteSize * s + glm::vec2(2.0f * s), kPanelFrame);
    ui.DrawRect(paletteOrigin, paletteSize * s, kPanelFill);
    ui.DrawText(paletteOrigin + glm::vec2(8.0f * s, 6.0f * s), "Blocks", UiTextScale(ui, s),
                kTitleColor);
    for (size_t i = 0; i < palette.size(); ++i) {
        const glm::vec2 pos =
            paletteOrigin + glm::vec2(kSlotsX + static_cast<float>(i % kPaletteColumns) *
                                                    kSlotPitch,
                                      kPaletteTitleBand +
                                          static_cast<float>(i / kPaletteColumns) * kSlotPitch) *
                                s;
        ui.DrawRect(pos, glm::vec2(16.0f * s), kSlotFill);
        DrawItemStack(ui, pos, s, {palette[i], 1});
        if (Hover(mouse, pos, glm::vec2(16.0f * s))) {
            ui.DrawRect(pos, glm::vec2(16.0f * s), kHoverHighlight);
            tooltip = BlockRegistry::Get().Def(palette[i]).name;
            // The palette is the free creative source: left grabs a full
            // stack (replacing whatever was carried), right adds one.
            if (leftClick) {
                carried = {palette[i], kMaxStackSize};
            } else if (rightClick) {
                if (carried.id == palette[i] && !carried.Empty()) {
                    carried.count = std::min(carried.count + 1, kMaxStackSize);
                } else {
                    carried = {palette[i], 1};
                }
            }
        }
    }

    // Inventory panel.
    if (tex.inventory) {
        ui.DrawImage(tex.inventory, panelOrigin, kPanelSize * s, {0.0f, 0.0f}, kPanelSize);
    } else {
        ui.DrawRect(panelOrigin - glm::vec2(s), kPanelSize * s + glm::vec2(2.0f * s), kPanelFrame);
        ui.DrawRect(panelOrigin, kPanelSize * s, kPanelFill);
        for (size_t i = 0; i < Inventory::kSize; ++i) {
            ui.DrawRect(panelOrigin + SlotPos(i) * s, glm::vec2(16.0f * s), kSlotFill);
        }
    }
    for (size_t i = 0; i < Inventory::kSize; ++i) {
        const glm::vec2 pos = panelOrigin + SlotPos(i) * s;
        DrawItemStack(ui, pos, s, inv.Slot(i));
        if (Hover(mouse, pos, glm::vec2(16.0f * s))) {
            ui.DrawRect(pos, glm::vec2(16.0f * s), kHoverHighlight);
            if (!inv.Slot(i).Empty()) {
                tooltip = BlockRegistry::Get().Def(inv.Slot(i).id).name;
            }
            if (click) {
                ClickSlot(inv.Slot(i), carried, rightClick);
            }
        }
    }

    // A click outside both panels discards the carried stack — the
    // creative-consistent stand-in for vanilla's throw until M18 gives us
    // item entities to drop.
    if (click && !carried.Empty() &&
        !Hover(mouse, paletteOrigin, paletteSize * s) &&
        !Hover(mouse, panelOrigin, kPanelSize * s)) {
        GAME_INFO("Discarded carried {} x{}", BlockRegistry::Get().Def(carried.id).name,
                  carried.count);
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
