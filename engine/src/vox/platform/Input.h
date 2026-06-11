#pragma once

#include <glm/vec2.hpp>

#include "vox/platform/KeyCodes.h"

namespace vox {

class Window;

// Immediate-mode polling against the active window. Attached by Application;
// game code just calls the static queries.
class Input {
public:
    static bool IsKeyDown(Key key);
    static bool IsMouseButtonDown(MouseButton button);
    static glm::vec2 MousePosition();

private:
    friend class Application;
    static void Attach(Window& window);

    static Window* s_window;
};

} // namespace vox
