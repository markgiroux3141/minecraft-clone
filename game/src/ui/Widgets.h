#pragma once

#include <string_view>

#include <glm/glm.hpp>

namespace vox {
class UiRenderer;
}

namespace vc {

// Immediate-mode building blocks shared by the menus (pause, title).

inline constexpr glm::vec4 kUiText{1.0f, 1.0f, 1.0f, 0.95f};
inline constexpr glm::vec4 kUiTextShadow{0.0f, 0.0f, 0.0f, 0.6f};

void ShadowedText(vox::UiRenderer& ui, glm::vec2 pos, std::string_view text, float scale,
                  glm::vec4 color = kUiText);

// Framed fill with a centered label; true when the click landed on it.
// s is the GUI scale (frame thickness and text scale derive from it).
bool UiButton(vox::UiRenderer& ui, float s, glm::vec2 pos, glm::vec2 size, std::string_view label,
              glm::vec2 mouse, bool clicked);

} // namespace vc
