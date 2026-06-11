#include "vox/platform/Input.h"

#include <GLFW/glfw3.h>

#include "vox/core/Assert.h"
#include "vox/platform/Window.h"

namespace vox {

Window* Input::s_window = nullptr;

void Input::Attach(Window& window) {
    s_window = &window;
}

bool Input::IsKeyDown(Key key) {
    VOX_ASSERT(s_window, "Input used before a window was attached");
    return glfwGetKey(s_window->Handle(), static_cast<int>(key)) == GLFW_PRESS;
}

bool Input::IsMouseButtonDown(MouseButton button) {
    VOX_ASSERT(s_window, "Input used before a window was attached");
    return glfwGetMouseButton(s_window->Handle(), static_cast<int>(button)) == GLFW_PRESS;
}

glm::vec2 Input::MousePosition() {
    VOX_ASSERT(s_window, "Input used before a window was attached");
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(s_window->Handle(), &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

} // namespace vox
