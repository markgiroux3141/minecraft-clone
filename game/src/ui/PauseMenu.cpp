#include "ui/PauseMenu.h"

#include <algorithm>
#include <string_view>

#include "vox/renderer/UiRenderer.h"

#include "ui/Hud.h"

namespace vc {

namespace {

constexpr glm::vec4 kDim{0.0f, 0.0f, 0.0f, 0.55f};
constexpr glm::vec4 kText{1.0f, 1.0f, 1.0f, 0.95f};
constexpr glm::vec4 kTextShadow{0.0f, 0.0f, 0.0f, 0.6f};
constexpr glm::vec4 kButtonFrame{0.1f, 0.1f, 0.1f, 0.9f};
constexpr glm::vec4 kButtonFrameHover{1.0f, 1.0f, 1.0f, 0.95f};
constexpr glm::vec4 kButtonFill{0.24f, 0.24f, 0.27f, 0.92f};
constexpr glm::vec4 kButtonFillHover{0.36f, 0.36f, 0.41f, 0.95f};

void ShadowedText(vox::UiRenderer& ui, glm::vec2 pos, std::string_view text, float scale,
                  glm::vec4 color) {
    ui.DrawText(pos + glm::vec2(scale), text, scale, kTextShadow);
    ui.DrawText(pos, text, scale, color);
}

bool Button(vox::UiRenderer& ui, float s, glm::vec2 pos, glm::vec2 size, std::string_view label,
            glm::vec2 mouse, bool clicked) {
    const bool hover = mouse.x >= pos.x && mouse.x < pos.x + size.x && mouse.y >= pos.y &&
                       mouse.y < pos.y + size.y;
    ui.DrawRect(pos, size, hover ? kButtonFrameHover : kButtonFrame);
    ui.DrawRect(pos + glm::vec2(s), size - glm::vec2(2.0f * s),
                hover ? kButtonFillHover : kButtonFill);

    const float textScale = std::max(1.0f, s - 1.0f);
    const glm::vec2 textSize = ui.MeasureText(label, textScale);
    ShadowedText(ui, glm::floor(pos + (size - textSize) * 0.5f), label, textScale, kText);
    return hover && clicked;
}

} // namespace

PauseMenu::Action PauseMenu::Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse,
                                  bool clicked) {
    const float s = GuiScale(screen);
    ui.DrawRect({0.0f, 0.0f}, screen, kDim);

    constexpr std::string_view kTitle = "Paused";
    const glm::vec2 titleSize = ui.MeasureText(kTitle, s);
    ShadowedText(ui, glm::floor(glm::vec2((screen.x - titleSize.x) * 0.5f, screen.y * 0.3f)),
                 kTitle, s, kText);

    const glm::vec2 buttonSize{160.0f * s, 24.0f * s};
    const float gap = 6.0f * s;
    const float x = std::floor((screen.x - buttonSize.x) * 0.5f);
    const float y = std::floor(screen.y * 0.45f);

    Action action = Action::None;
    if (Button(ui, s, {x, y}, buttonSize, "Resume", mouse, clicked)) {
        action = Action::Resume;
    }
    if (Button(ui, s, {x, y + buttonSize.y + gap}, buttonSize, "Save & Quit", mouse, clicked)) {
        action = Action::SaveQuit;
    }
    return action;
}

} // namespace vc
