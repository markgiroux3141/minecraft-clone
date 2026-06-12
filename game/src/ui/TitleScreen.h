#pragma once

#include <span>
#include <string>

#include <glm/glm.hpp>

#include "ui/Widgets.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// Immediate-mode title / world-select screen. Lists the worlds found in the
// saves directory; the caller owns the list and acts on the returned click.
class TitleScreen {
public:
    struct Action {
        enum class Type { None, Play, NewWorld, Quit };
        Type type = Type::None;
        size_t worldIndex = 0; // valid when type == Play
    };

    static Action Draw(vox::UiRenderer& ui, glm::vec2 screen, glm::vec2 mouse, bool clicked,
                       std::span<const std::string> worlds, const GuiTextures& tex);
};

} // namespace vc
