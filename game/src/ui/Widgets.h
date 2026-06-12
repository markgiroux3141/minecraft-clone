#pragma once

#include <memory>
#include <string_view>

#include <glm/glm.hpp>

namespace vox {
class Texture2D;
class UiRenderer;
}

namespace vc {

// Immediate-mode building blocks shared by the menus (pause, title).

// Optional real-Minecraft GUI sheets (assets/mc/, see import_mc_assets.py).
// Null members mean "draw the procedural placeholder look instead".
struct GuiTextures {
    std::shared_ptr<vox::Texture2D> icons;   // gui/icons.png — crosshair
    std::shared_ptr<vox::Texture2D> widgets; // gui/widgets.png — hotbar, buttons
};

inline constexpr glm::vec4 kUiText{1.0f, 1.0f, 1.0f, 0.95f};
inline constexpr glm::vec4 kUiTextShadow{0.0f, 0.0f, 0.0f, 0.6f};

// The placeholder Consolas font's 17 px cells run one scale behind the GUI;
// the real MC font's 8 px glyphs take the full scale.
float UiTextScale(const vox::UiRenderer& ui, float s);

void ShadowedText(vox::UiRenderer& ui, glm::vec2 pos, std::string_view text, float scale,
                  glm::vec4 color = kUiText);

// Centered-label button; true when the click landed on it. s is the GUI
// scale. With the widgets sheet it draws Minecraft's 200x20 button sprites
// (pass a 200x20-per-scale size for crisp pixels); otherwise framed rects.
bool UiButton(vox::UiRenderer& ui, float s, glm::vec2 pos, glm::vec2 size, std::string_view label,
              glm::vec2 mouse, bool clicked,
              const std::shared_ptr<vox::Texture2D>& widgets = nullptr);

} // namespace vc
