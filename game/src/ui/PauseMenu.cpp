#include "ui/PauseMenu.h"

#include <string_view>

#include "vox/renderer/UiRenderer.h"

#include "ui/Hud.h"
#include "ui/Widgets.h"

namespace vc {

namespace {

constexpr glm::vec4 kDim{0.0f, 0.0f, 0.0f, 0.55f};

} // namespace

PauseMenu::Action PauseMenu::Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse,
                                  bool clicked) {
    const float s = GuiScale(screen);
    ui.DrawRect({0.0f, 0.0f}, screen, kDim);

    constexpr std::string_view kTitle = "Paused";
    const glm::vec2 titleSize = ui.MeasureText(kTitle, s);
    ShadowedText(ui, glm::floor(glm::vec2((screen.x - titleSize.x) * 0.5f, screen.y * 0.3f)),
                 kTitle, s);

    const glm::vec2 buttonSize{160.0f * s, 24.0f * s};
    const float gap = 6.0f * s;
    const float x = std::floor((screen.x - buttonSize.x) * 0.5f);
    const float y = std::floor(screen.y * 0.45f);

    Action action = Action::None;
    if (UiButton(ui, s, {x, y}, buttonSize, "Resume", mouse, clicked)) {
        action = Action::Resume;
    }
    if (UiButton(ui, s, {x, y + buttonSize.y + gap}, buttonSize, "Save & Quit to Title", mouse,
                 clicked)) {
        action = Action::SaveQuit;
    }
    return action;
}

} // namespace vc
