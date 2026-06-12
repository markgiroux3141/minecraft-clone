#include "ui/TitleScreen.h"

#include <algorithm>
#include <string_view>

#include "vox/renderer/UiRenderer.h"

#include "ui/Hud.h"
#include "ui/Widgets.h"

namespace vc {

namespace {

// Keeps the bottom buttons on screen with many saves; scrolling can come
// when someone actually has more worlds than this.
constexpr size_t kMaxListedWorlds = 6;

} // namespace

TitleScreen::Action TitleScreen::Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse,
                                      bool clicked, std::span<const std::string> worlds,
                                      const GuiTextures& tex) {
    const float s = GuiScale(screen);

    constexpr std::string_view kTitle = "VOXCRAFT";
    const float titleScale = UiTextScale(ui, s) + 1.0f;
    const glm::vec2 titleSize = ui.MeasureText(kTitle, titleScale);
    ShadowedText(ui, glm::floor(glm::vec2((screen.x - titleSize.x) * 0.5f, screen.y * 0.14f)),
                 kTitle, titleScale);

    // Minecraft's standard button is 200x20 — the widgets.png sprite size.
    const glm::vec2 buttonSize{200.0f * s, 20.0f * s};
    const float gap = 4.0f * s;
    const float x = std::floor((screen.x - buttonSize.x) * 0.5f);
    float y = std::floor(screen.y * 0.32f);

    Action action;
    const size_t listed = std::min(worlds.size(), kMaxListedWorlds);
    for (size_t i = 0; i < listed; ++i) {
        if (UiButton(ui, s, {x, y}, buttonSize, worlds[i], mouse, clicked, tex.widgets)) {
            action = {Action::Type::Play, i};
        }
        y += buttonSize.y + gap;
    }

    y += 8.0f * s; // breathing room between the world list and the actions
    if (UiButton(ui, s, {x, y}, buttonSize, "New World", mouse, clicked, tex.widgets)) {
        action.type = Action::Type::NewWorld;
    }
    y += buttonSize.y + gap;
    if (UiButton(ui, s, {x, y}, buttonSize, "Quit", mouse, clicked, tex.widgets)) {
        action.type = Action::Type::Quit;
    }
    return action;
}

} // namespace vc
