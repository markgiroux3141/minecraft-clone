#include "vox/platform/Window.h"

#include <stdexcept>

#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include "vox/core/Assert.h"
#include "vox/core/Log.h"

namespace vox {

namespace {

uint32_t s_windowCount = 0;
bool s_glLoaded = false;

void GlfwErrorCallback(int code, const char* description) {
    VOX_ERROR("GLFW error {}: {}", code, description);
}

} // namespace

Window::Window(const WindowConfig& config) : m_config(config) {
    if (s_windowCount == 0) {
        glfwSetErrorCallback(GlfwErrorCallback);
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef VOX_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    m_window = glfwCreateWindow(static_cast<int>(m_config.width),
                                static_cast<int>(m_config.height), m_config.title.c_str(),
                                nullptr, nullptr);
    if (!m_window) {
        throw std::runtime_error("Failed to create window (OpenGL 4.6 support required)");
    }
    ++s_windowCount;

    glfwMakeContextCurrent(m_window);

    if (!s_glLoaded) {
        const int version = gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
        if (version == 0) {
            throw std::runtime_error("Failed to load OpenGL function pointers");
        }
        s_glLoaded = true;
    }

    SetVsync(m_config.vsync);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetScrollCallback(m_window, [](GLFWwindow* handle, double /*x*/, double y) {
        static_cast<Window*>(glfwGetWindowUserPointer(handle))->m_scrollY += y;
    });
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* handle, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(handle));
        self->m_config.width = static_cast<uint32_t>(width);
        self->m_config.height = static_cast<uint32_t>(height);
        if (self->m_resizeCallback) {
            self->m_resizeCallback(self->m_config.width, self->m_config.height);
        }
    });

    VOX_INFO("Window created: {}x{} (vsync {})", m_config.width, m_config.height,
             m_config.vsync ? "on" : "off");
}

Window::~Window() {
    glfwDestroyWindow(m_window);
    if (--s_windowCount == 0) {
        glfwTerminate();
    }
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SwapBuffers() {
    glfwSwapBuffers(m_window);
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void Window::RequestClose() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void Window::SetVsync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
    m_config.vsync = enabled;
}

void Window::SetCursorCaptured(bool captured) {
    if (captured == m_cursorCaptured) {
        return;
    }
    m_cursorCaptured = captured;
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

double Window::TakeScrollY() {
    const double y = m_scrollY;
    m_scrollY = 0.0;
    return y;
}

void Window::SetTitle(const std::string& title) {
    m_config.title = title;
    glfwSetWindowTitle(m_window, title.c_str());
}

} // namespace vox
