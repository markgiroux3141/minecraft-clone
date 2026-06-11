#include "ui/Widgets.h"

#include <algorithm>

#include "vox/renderer/UiRenderer.h"

namespace vc {

namespace {

constexpr glm::vec4 kButtonFrame{0.1f, 0.1f, 0.1f, 0.9f};
constexpr glm::vec4 kButtonFrameHover{1.0f, 1.0f, 1.0f, 0.95f};
constexpr glm::vec4 kButtonFill{0.24f, 0.24f, 0.27f, 0.92f};
constexpr glm::vec4 kButtonFillHover{0.36f, 0.36f, 0.41f, 0.95f};

} // namespace

void ShadowedText(vox::UiRenderer& ui, glm::vec2 pos, std::string_view text, float scale,
                  glm::vec4 color) {
    ui.DrawText(pos + glm::vec2(scale), text, scale, kUiTextShadow);
    ui.DrawText(pos, text, scale, color);
}

bool UiButton(vox::UiRenderer& ui, float s, glm::vec2 pos, glm::vec2 size, std::string_view label,
              glm::vec2 mouse, bool clicked) {
    const bool hover = mouse.x >= pos.x && mouse.x < pos.x + size.x && mouse.y >= pos.y &&
                       mouse.y < pos.y + size.y;
    ui.DrawRect(pos, size, hover ? kButtonFrameHover : kButtonFrame);
    ui.DrawRect(pos + glm::vec2(s), size - glm::vec2(2.0f * s),
                hover ? kButtonFillHover : kButtonFill);

    const float textScale = std::max(1.0f, s - 1.0f);
    const glm::vec2 textSize = ui.MeasureText(label, textScale);
    ShadowedText(ui, glm::floor(pos + (size - textSize) * 0.5f), label, textScale);
    return hover && clicked;
}

} // namespace vc
