#pragma once

#include <glm/glm.hpp>

#include "ui/Widgets.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// Immediate-mode pause menu drawn over the dimmed scene. Draw() renders and
// reports which button the given click landed on; the caller owns the game
// state and the cursor.
class PauseMenu {
public:
    enum class Action { None, Resume, SaveQuit };

    // mouse is the cursor position in framebuffer pixels; clicked is the
    // left button's press edge for this frame.
    static Action Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse, bool clicked,
                       const GuiTextures& tex);
};

} // namespace vc
