#include "ui/Hud.h"

#include <algorithm>

#include "vox/renderer/UiRenderer.h"

namespace vc {

namespace {

constexpr glm::vec4 kWhite{1.0f, 1.0f, 1.0f, 0.92f};
constexpr glm::vec4 kShadow{0.0f, 0.0f, 0.0f, 0.45f};
constexpr glm::vec4 kSlotBg{0.0f, 0.0f, 0.0f, 0.55f};
constexpr glm::vec4 kSlotFrame{0.55f, 0.55f, 0.55f, 0.85f};

void DrawCrosshair(vox::UiRenderer& ui, glm::vec2 screen, float s, const GuiTextures& tex) {
    // Integer center so the cross lands on whole pixels.
    const glm::vec2 c{std::floor(screen.x * 0.5f), std::floor(screen.y * 0.5f)};
    if (tex.icons) {
        // icons.png crosshair, drawn at MC's -7,-7 offset. (Vanilla uses an
        // inverted blend; plain alpha here — may wash out on pale blocks.)
        ui.DrawImage(tex.icons, c - 7.0f * s, glm::vec2(16.0f * s), {0.0f, 0.0f}, {16.0f, 16.0f});
        return;
    }
    const float half = 5.0f * s;
    const float t = s;
    // Soft dark backing one pixel proud keeps it visible over bright blocks.
    ui.DrawRect({c.x - half - 1.0f, c.y - t * 0.5f - 1.0f}, {2.0f * half + 2.0f, t + 2.0f},
                kShadow);
    ui.DrawRect({c.x - t * 0.5f - 1.0f, c.y - half - 1.0f}, {t + 2.0f, 2.0f * half + 2.0f},
                kShadow);
    ui.DrawRect({c.x - half, c.y - t * 0.5f}, {2.0f * half, t}, kWhite);
    ui.DrawRect({c.x - t * 0.5f, c.y - half}, {t, 2.0f * half}, kWhite);
}

// widgets.png layout: hotbar background 182x22 at (0,0) — 1px border, nine
// 20px slots; selection frame 24x22 at (0,22), drawn one pixel proud.
void DrawMcHotbar(vox::UiRenderer& ui, glm::vec2 screen, float s,
                  std::span<const ItemStack> hotbar, size_t selectedSlot,
                  const GuiTextures& tex) {
    const glm::vec2 origin{std::floor((screen.x - 182.0f * s) * 0.5f), screen.y - 22.0f * s};
    ui.DrawImage(tex.widgets, origin, {182.0f * s, 22.0f * s}, {0.0f, 0.0f}, {182.0f, 22.0f});
    ui.DrawImage(tex.widgets,
                 origin + glm::vec2((static_cast<float>(selectedSlot) * 20.0f - 1.0f) * s,
                                    -1.0f * s),
                 {24.0f * s, 22.0f * s}, {0.0f, 22.0f}, {24.0f, 22.0f});

    const size_t slots = std::min(hotbar.size(), size_t{9});
    for (size_t i = 0; i < slots; ++i) {
        DrawItemStack(ui,
                      origin + glm::vec2((3.0f + static_cast<float>(i) * 20.0f) * s, 3.0f * s), s,
                      hotbar[i]);
    }
}

void DrawPlaceholderHotbar(vox::UiRenderer& ui, glm::vec2 screen, float s,
                           std::span<const ItemStack> hotbar, size_t selectedSlot) {
    const float slot = 22.0f * s; // 16x16 icon + 3px border each side, pre-scale
    const float gap = 2.0f * s;
    const auto count = static_cast<float>(hotbar.size());
    const glm::vec2 origin{std::floor((screen.x - count * slot - (count - 1.0f) * gap) * 0.5f),
                           screen.y - slot - 6.0f * s};

    for (size_t i = 0; i < hotbar.size(); ++i) {
        const glm::vec2 pos{origin.x + static_cast<float>(i) * (slot + gap), origin.y};
        const bool selected = i == selectedSlot;
        if (selected) {
            ui.DrawRect(pos - glm::vec2(s), glm::vec2(slot + 2.0f * s), kWhite);
        }
        ui.DrawRect(pos, glm::vec2(slot), selected ? kSlotBg : kSlotFrame);
        ui.DrawRect(pos + glm::vec2(s), glm::vec2(slot - 2.0f * s), kSlotBg);
        DrawItemStack(ui, pos + glm::vec2(3.0f * s), s, hotbar[i]);
    }
}

void DrawSelectedName(vox::UiRenderer& ui, glm::vec2 screen, float s,
                      std::span<const ItemStack> hotbar, size_t selectedSlot, float barTop) {
    const float textScale = UiTextScale(ui, s);
    const std::string& name = ItemName(hotbar[selectedSlot].id);
    const glm::vec2 size = ui.MeasureText(name, textScale);
    const glm::vec2 textPos{std::floor((screen.x - size.x) * 0.5f), barTop - size.y - 4.0f * s};
    ui.DrawText(textPos + glm::vec2(textScale), name, textScale, {0.0f, 0.0f, 0.0f, 0.6f});
    ui.DrawText(textPos, name, textScale, kWhite);
}

} // namespace

float GuiScale(glm::vec2 screen) {
    return std::max(1.0f,
                    std::min(std::floor(screen.x / 320.0f), std::floor(screen.y / 240.0f)));
}

void Hud::Draw(vox::UiRenderer& ui, glm::vec2 screen, std::span<const ItemStack> hotbar,
               size_t selectedSlot, const GuiTextures& tex) {
    const float s = GuiScale(screen);
    DrawCrosshair(ui, screen, s, tex);
    if (hotbar.empty()) {
        return;
    }
    const bool named = !hotbar[selectedSlot].Empty(); // empty hand: no label
    if (tex.widgets) {
        DrawMcHotbar(ui, screen, s, hotbar, selectedSlot, tex);
        if (named) {
            DrawSelectedName(ui, screen, s, hotbar, selectedSlot, screen.y - 22.0f * s);
        }
    } else {
        DrawPlaceholderHotbar(ui, screen, s, hotbar, selectedSlot);
        if (named) {
            DrawSelectedName(ui, screen, s, hotbar, selectedSlot, screen.y - 28.0f * s);
        }
    }
}

} // namespace vc
