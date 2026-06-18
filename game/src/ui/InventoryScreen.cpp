#include "ui/InventoryScreen.h"

#include <algorithm>
#include <string>
#include <vector>

#include "vox/renderer/Texture.h"
#include "vox/renderer/UiRenderer.h"

#include "item/Crafting.h"
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
// M33: the four worn-armor slots down the left edge (vanilla ContainerPlayer:
// x=8, y=8 + slot*18), and the player-doll box between them and the craft grid.
constexpr glm::vec2 kArmorPos{8.0f, 8.0f};
constexpr glm::vec2 kDollPos{26.0f, 8.0f};
constexpr glm::vec2 kDollSize = InventoryScreen::kDollBoxSize;
constexpr glm::vec2 kTableCraftPos{30.0f, 17.0f};
constexpr glm::vec2 kTableResultPos{124.0f, 35.0f};
// ContainerFurnace: input over fuel on the left, output on the right;
// GuiFurnace's flame/arrow overlays live right of the panel in the sheet.
constexpr glm::vec2 kFurnaceInputPos{56.0f, 17.0f};
constexpr glm::vec2 kFurnaceFuelPos{56.0f, 53.0f};
constexpr glm::vec2 kFurnaceOutputPos{116.0f, 35.0f};
constexpr float kPanelGap = 4.0f; // between the palette and inventory panels

constexpr int kPaletteColumns = 9;
constexpr int kPaletteVisibleRows = 6;     // window height; the rest scrolls
constexpr float kPaletteTitleBand = 17.0f; // title row above the entries
constexpr float kPaletteBottomPad = 7.0f;
constexpr float kScrollbarWidth = 4.0f; // thin thumb track on the right edge

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

// M33 armor slots (vanilla SlotArmor): only the matching armor type goes in,
// but the worn piece can always be taken out. Armor stacks to 1, so a swap
// covers equip/unequip for both mouse buttons.
void ClickArmorSlot(ItemStack& slot, ItemStack& carried, ArmorSlot which) {
    if (carried.Empty() || (IsArmor(carried.id) && ArmorSlotOf(carried.id) == which)) {
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
                           const GuiTextures& tex, int& paletteScroll) {
    const float s = GuiScale(screen);
    const bool table = craftSize == 3;
    const bool click = leftClick || rightClick;
    std::string tooltip;

    // The player screen carries the creative palette above the panel; the
    // crafting table is just the panel. The palette is a fixed-height scrolling
    // WINDOW (kPaletteVisibleRows) so it never overflows however many items
    // exist; paletteScroll (rows, owned by GameApp + advanced by the wheel) is
    // clamped here. Center the stack either way.
    const std::vector<ItemId> palette = table ? std::vector<ItemId>{} : PaletteIds();
    const int totalRows =
        static_cast<int>((palette.size() + kPaletteColumns - 1) / kPaletteColumns);
    const int visibleRows = std::min(totalRows, kPaletteVisibleRows);
    const int maxScroll = std::max(0, totalRows - visibleRows);
    if (table) {
        paletteScroll = 0;
    } else {
        paletteScroll = std::clamp(paletteScroll, 0, maxScroll);
    }
    const glm::vec2 paletteSize{
        kPanelSize.x, table ? 0.0f
                            : kPaletteTitleBand + static_cast<float>(visibleRows) * kSlotPitch +
                                  kPaletteBottomPad};
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
        // Only the visible window of rows draws + hit-tests.
        const size_t first = static_cast<size_t>(paletteScroll) * kPaletteColumns;
        const size_t last = std::min(palette.size(),
                                     first + static_cast<size_t>(visibleRows) * kPaletteColumns);
        for (size_t i = first; i < last; ++i) {
            const size_t vis = i - first; // index within the visible window
            const glm::vec2 pos =
                paletteOrigin +
                glm::vec2(kSlotsX + static_cast<float>(vis % kPaletteColumns) * kSlotPitch,
                          kPaletteTitleBand +
                              static_cast<float>(vis / kPaletteColumns) * kSlotPitch) *
                    s;
            ui.DrawRect(pos, glm::vec2(16.0f * s), kSlotFill);
            DrawItemStack(ui, pos, s, {palette[i], 1}, tex.blockIcons);
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
        // Scrollbar thumb on the right edge (only when there's overflow).
        if (maxScroll > 0) {
            const float trackX = paletteOrigin.x + (kPanelSize.x - kScrollbarWidth - 1.0f) * s;
            const float trackY = paletteOrigin.y + kPaletteTitleBand * s;
            const float trackH = static_cast<float>(visibleRows) * kSlotPitch * s;
            ui.DrawRect({trackX, trackY}, {kScrollbarWidth * s, trackH}, kSlotFill);
            const float thumbH = trackH * static_cast<float>(visibleRows) /
                                 static_cast<float>(totalRows);
            const float thumbY = trackY + (trackH - thumbH) * static_cast<float>(paletteScroll) /
                                              static_cast<float>(maxScroll);
            ui.DrawRect({trackX, thumbY}, {kScrollbarWidth * s, thumbH}, kTitleColor);
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
        if (!table) {
            for (int a = 0; a < kArmorSlots; ++a) {
                ui.DrawRect(panelOrigin +
                                (kArmorPos + glm::vec2(0.0f, static_cast<float>(a) * kSlotPitch)) *
                                    s,
                            glm::vec2(16.0f * s), kSlotFill);
            }
        }
    }

    // A regular interactive slot (inventory and craft cells alike).
    const auto doSlot = [&](ItemStack& stack, glm::vec2 pos) {
        DrawItemStack(ui, pos, s, stack, tex.blockIcons);
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

    // M33: the player doll (drawn under the armor icons), then the four armor
    // slots — empty ones show their placeholder sprite; equip is type-gated.
    if (!table) {
        if (tex.playerDoll) {
            const glm::vec2 texSize{static_cast<float>(tex.playerDoll->Width()),
                                    static_cast<float>(tex.playerDoll->Height())};
            ui.DrawImage(tex.playerDoll, panelOrigin + kDollPos * s, kDollSize * s, {0.0f, 0.0f},
                         texSize);
        }
        for (int a = 0; a < kArmorSlots; ++a) {
            const glm::vec2 pos =
                panelOrigin + (kArmorPos + glm::vec2(0.0f, static_cast<float>(a) * kSlotPitch)) * s;
            ItemStack& piece = inv.Armor(static_cast<size_t>(a));
            if (piece.Empty()) {
                ui.DrawAtlasTile(pos, glm::vec2(16.0f * s), kEmptyArmorSlotTile[a]);
            }
            DrawItemStack(ui, pos, s, piece, tex.blockIcons);
            if (Hover(mouse, pos, glm::vec2(16.0f * s))) {
                ui.DrawRect(pos, glm::vec2(16.0f * s), kHoverHighlight);
                if (!piece.Empty()) {
                    tooltip = ItemName(piece.id);
                }
                if (click) {
                    ClickArmorSlot(piece, carried, static_cast<ArmorSlot>(a));
                }
            }
        }
    }

    // Result slot: shows the recipe match; clicking crafts into the
    // cursor (vanilla: only when it fits) and consumes one per grid cell.
    const ItemStack result = Recipes::Match(craft, craftSize);
    const glm::vec2 resultDraw = panelOrigin + resultPos * s;
    DrawItemStack(ui, resultDraw, s, result, tex.blockIcons);
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
        DrawItemStack(ui, mouse - glm::vec2(8.0f * s), s, carried, tex.blockIcons);
    } else if (!tooltip.empty()) {
        const float textScale = UiTextScale(ui, s);
        const glm::vec2 size = ui.MeasureText(tooltip, textScale);
        const glm::vec2 pos = mouse + glm::vec2(12.0f * s, -12.0f * s);
        ui.DrawRect(pos - glm::vec2(3.0f * s), size + glm::vec2(6.0f * s), kTooltipFill);
        ShadowedText(ui, pos, tooltip, textScale);
    }
}

void InventoryScreen::DrawFurnace(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse,
                                  bool leftClick, bool rightClick, Inventory& inv,
                                  FurnaceState& furnace, ItemStack& carried, ItemStack& thrown,
                                  const GuiTextures& tex) {
    const float s = GuiScale(screen);
    const bool click = leftClick || rightClick;
    std::string tooltip;

    const glm::vec2 panelOrigin{std::floor((screen.x - kPanelSize.x * s) * 0.5f),
                                std::floor((screen.y - kPanelSize.y * s) * 0.5f)};

    if (tex.furnace) {
        ui.DrawImage(tex.furnace, panelOrigin, kPanelSize * s, {0.0f, 0.0f}, kPanelSize);
    } else {
        ui.DrawRect(panelOrigin - glm::vec2(s), kPanelSize * s + glm::vec2(2.0f * s),
                    kPanelFrame);
        ui.DrawRect(panelOrigin, kPanelSize * s, kPanelFill);
        ui.DrawText(panelOrigin + glm::vec2(60.0f * s, 6.0f * s), "Furnace", UiTextScale(ui, s),
                    kTitleColor);
        for (size_t i = 0; i < Inventory::kSize; ++i) {
            ui.DrawRect(panelOrigin + SlotPos(i) * s, glm::vec2(16.0f * s), kSlotFill);
        }
        for (const glm::vec2 pos : {kFurnaceInputPos, kFurnaceFuelPos, kFurnaceOutputPos}) {
            ui.DrawRect(panelOrigin + pos * s, glm::vec2(16.0f * s), kSlotFill);
        }
    }

    // Progress overlays, GuiFurnace's sub-rects: the flame climbs 13px
    // with burn time left, the arrow sweeps 24px with cook progress. The
    // sheet keeps both right of the panel art at x 176.
    const int flame = furnace.burnTicks > 0
                          ? furnace.burnTicks * 13 / std::max(furnace.burnTotal, 1)
                          : -1;
    const int arrow = furnace.cookTicks * 24 / furnace::kCookTicks;
    if (tex.furnace) {
        if (flame >= 0) {
            ui.DrawImage(tex.furnace,
                         panelOrigin + glm::vec2(56.0f, 36.0f + 12.0f - static_cast<float>(flame)) * s,
                         glm::vec2(14.0f, static_cast<float>(flame + 1)) * s,
                         {176.0f, 12.0f - static_cast<float>(flame)},
                         {14.0f, static_cast<float>(flame + 1)});
        }
        if (arrow > 0) {
            ui.DrawImage(tex.furnace, panelOrigin + glm::vec2(79.0f, 34.0f) * s,
                         glm::vec2(static_cast<float>(arrow + 1), 16.0f) * s, {176.0f, 14.0f},
                         {static_cast<float>(arrow + 1), 16.0f});
        }
    } else {
        if (flame >= 0) {
            ui.DrawRect(panelOrigin + glm::vec2(57.0f, 36.0f + 12.0f - static_cast<float>(flame)) * s,
                        glm::vec2(12.0f, static_cast<float>(flame + 1)) * s,
                        {0.95f, 0.55f, 0.12f, 1.0f});
        }
        if (arrow > 0) {
            ui.DrawRect(panelOrigin + glm::vec2(79.0f, 40.0f) * s,
                        glm::vec2(static_cast<float>(arrow), 4.0f) * s,
                        {0.95f, 0.95f, 0.95f, 1.0f});
        }
    }

    // Inventory and the input/fuel slots take normal clicks; the output
    // slot is take-only (vanilla SlotFurnaceOutput).
    const auto doSlot = [&](ItemStack& stack, glm::vec2 pos) {
        DrawItemStack(ui, pos, s, stack, tex.blockIcons);
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
    doSlot(furnace.input, panelOrigin + kFurnaceInputPos * s);
    doSlot(furnace.fuel, panelOrigin + kFurnaceFuelPos * s);

    {
        ItemStack& out = furnace.output;
        const glm::vec2 pos = panelOrigin + kFurnaceOutputPos * s;
        DrawItemStack(ui, pos, s, out, tex.blockIcons);
        if (Hover(mouse, pos, glm::vec2(16.0f * s))) {
            ui.DrawRect(pos, glm::vec2(16.0f * s), kHoverHighlight);
            if (!out.Empty()) {
                tooltip = ItemName(out.id);
            }
            if (click && !out.Empty()) {
                if (carried.Empty()) {
                    carried = out;
                    out = {};
                } else if (SameKind(carried, out)) {
                    const int moved = std::min(out.count, ItemMaxStack(out.id) - carried.count);
                    carried.count += std::max(moved, 0);
                    out.count -= std::max(moved, 0);
                    if (out.count == 0) {
                        out = {};
                    }
                }
            }
        }
    }

    // A click outside the panel throws the carried stack, like the other
    // container screens.
    if (click && !carried.Empty() && !Hover(mouse, panelOrigin, kPanelSize * s)) {
        thrown = carried;
        carried = {};
    }

    if (!carried.Empty()) {
        DrawItemStack(ui, mouse - glm::vec2(8.0f * s), s, carried, tex.blockIcons);
    } else if (!tooltip.empty()) {
        const float textScale = UiTextScale(ui, s);
        const glm::vec2 size = ui.MeasureText(tooltip, textScale);
        const glm::vec2 pos = mouse + glm::vec2(12.0f * s, -12.0f * s);
        ui.DrawRect(pos - glm::vec2(3.0f * s), size + glm::vec2(6.0f * s), kTooltipFill);
        ShadowedText(ui, pos, tooltip, textScale);
    }
}

} // namespace vc
