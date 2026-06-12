#include "ui/Widgets.h"

#include <algorithm>

#include "vox/renderer/UiRenderer.h"

namespace vc {

namespace {

constexpr glm::vec4 kButtonFrame{0.1f, 0.1f, 0.1f, 0.9f};
constexpr glm::vec4 kButtonFrameHover{1.0f, 1.0f, 1.0f, 0.95f};
constexpr glm::vec4 kButtonFill{0.24f, 0.24f, 0.27f, 0.92f};
constexpr glm::vec4 kButtonFillHover{0.36f, 0.36f, 0.41f, 0.95f};

// Minecraft's hovered-button label color (0xFFFFA0).
constexpr glm::vec4 kLabelHover{1.0f, 1.0f, 0.63f, 1.0f};

// widgets.png: 200x20 button sprites — y 66 normal, y 86 hovered.
constexpr glm::vec2 kButtonSprite{200.0f, 20.0f};

} // namespace

float UiTextScale(const vox::UiRenderer& ui, float s) {
    return ui.GlyphSize().y <= 8.0f ? s : std::max(1.0f, s - 1.0f);
}

void ShadowedText(vox::UiRenderer& ui, glm::vec2 pos, std::string_view text, float scale,
                  glm::vec4 color) {
    ui.DrawText(pos + glm::vec2(scale), text, scale, kUiTextShadow);
    ui.DrawText(pos, text, scale, color);
}

bool UiButton(vox::UiRenderer& ui, float s, glm::vec2 pos, glm::vec2 size, std::string_view label,
              glm::vec2 mouse, bool clicked,
              const std::shared_ptr<vox::Texture2D>& widgets) {
    const bool hover = mouse.x >= pos.x && mouse.x < pos.x + size.x && mouse.y >= pos.y &&
                       mouse.y < pos.y + size.y;
    if (widgets) {
        ui.DrawImage(widgets, pos, size, {0.0f, hover ? 86.0f : 66.0f}, kButtonSprite);
    } else {
        ui.DrawRect(pos, size, hover ? kButtonFrameHover : kButtonFrame);
        ui.DrawRect(pos + glm::vec2(s), size - glm::vec2(2.0f * s),
                    hover ? kButtonFillHover : kButtonFill);
    }

    const float textScale = UiTextScale(ui, s);
    const glm::vec2 textSize = ui.MeasureText(label, textScale);
    ShadowedText(ui, glm::floor(pos + (size - textSize) * 0.5f), label, textScale,
                 widgets && hover ? kLabelHover : kUiText);
    return hover && clicked;
}

void DrawItemStack(vox::UiRenderer& ui, glm::vec2 pos, float s, const ItemStack& stack) {
    if (stack.Empty()) {
        return;
    }
    // Side face: for grass that's the fringe tile, more recognizable than
    // plain green from the top.
    const uint16_t layer =
        BlockRegistry::Get().Def(stack.id).faceTiles[static_cast<size_t>(BlockFace::PosX)];
    ui.DrawAtlasTile(pos, glm::vec2(16.0f * s), layer);
    if (stack.count > 1) {
        // Vanilla anchors the count's bottom-right one pixel proud of the
        // icon (right edge x+17, bottom y+17 with the 8px font).
        const std::string text = std::to_string(stack.count);
        const float textScale = UiTextScale(ui, s);
        const glm::vec2 size = ui.MeasureText(text, textScale);
        ShadowedText(ui, glm::floor(pos + glm::vec2(17.0f * s) - size), text, textScale);
    }
}

} // namespace vc
