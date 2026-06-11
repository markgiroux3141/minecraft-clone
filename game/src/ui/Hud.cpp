#include "ui/Hud.h"

#include <algorithm>

#include "vox/renderer/UiRenderer.h"

namespace vc {

namespace {

constexpr glm::vec4 kWhite{1.0f, 1.0f, 1.0f, 0.92f};
constexpr glm::vec4 kShadow{0.0f, 0.0f, 0.0f, 0.45f};
constexpr glm::vec4 kSlotBg{0.0f, 0.0f, 0.0f, 0.55f};
constexpr glm::vec4 kSlotFrame{0.55f, 0.55f, 0.55f, 0.85f};

void DrawCrosshair(vox::UiRenderer& ui, glm::vec2 screen, float s) {
    // Integer center so the cross lands on whole pixels.
    const glm::vec2 c{std::floor(screen.x * 0.5f), std::floor(screen.y * 0.5f)};
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

void DrawHotbar(vox::UiRenderer& ui, glm::vec2 screen, float s,
                std::span<const BlockId> hotbar, size_t selectedSlot) {
    const float slot = 22.0f * s; // 16x16 icon + 3px border each side, pre-scale
    const float gap = 2.0f * s;
    const auto count = static_cast<float>(hotbar.size());
    const glm::vec2 origin{std::floor((screen.x - count * slot - (count - 1.0f) * gap) * 0.5f),
                           screen.y - slot - 6.0f * s};

    const auto& registry = BlockRegistry::Get();
    for (size_t i = 0; i < hotbar.size(); ++i) {
        const glm::vec2 pos{origin.x + static_cast<float>(i) * (slot + gap), origin.y};
        const bool selected = i == selectedSlot;
        if (selected) {
            ui.DrawRect(pos - glm::vec2(s), glm::vec2(slot + 2.0f * s), kWhite);
        }
        ui.DrawRect(pos, glm::vec2(slot), selected ? kSlotBg : kSlotFrame);
        ui.DrawRect(pos + glm::vec2(s), glm::vec2(slot - 2.0f * s), kSlotBg);
        if (hotbar[i] != blocks::Air) {
            // Side face: for grass that's the fringe tile, more recognizable
            // than plain green from the top.
            const uint16_t layer =
                registry.Def(hotbar[i]).faceTiles[static_cast<size_t>(BlockFace::PosX)];
            ui.DrawAtlasTile(pos + glm::vec2(3.0f * s), glm::vec2(16.0f * s), layer);
        }
    }

    // Selected block name, shadowed, centered above the bar. The font's 17 px
    // cells are ~2x Minecraft's 8 px glyphs, so text runs one scale behind.
    const float textScale = std::max(1.0f, s - 1.0f);
    const std::string& name = registry.Def(hotbar[selectedSlot]).name;
    const glm::vec2 size = ui.MeasureText(name, textScale);
    const glm::vec2 textPos{std::floor((screen.x - size.x) * 0.5f),
                            origin.y - size.y - 4.0f * s};
    ui.DrawText(textPos + glm::vec2(textScale), name, textScale, {0.0f, 0.0f, 0.0f, 0.6f});
    ui.DrawText(textPos, name, textScale, kWhite);
}

} // namespace

void Hud::Draw(vox::UiRenderer& ui, glm::vec2 screen, std::span<const BlockId> hotbar,
               size_t selectedSlot) {
    // Minecraft's auto GUI scale: the largest integer scale that keeps a
    // 320x240 layout on screen (3 at 1600x900, 4 at 4k).
    const float s = std::max(1.0f, std::min(std::floor(screen.x / 320.0f),
                                            std::floor(screen.y / 240.0f)));
    DrawCrosshair(ui, screen, s);
    if (!hotbar.empty()) {
        DrawHotbar(ui, screen, s, hotbar, selectedSlot);
    }
}

} // namespace vc
