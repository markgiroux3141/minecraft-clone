#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct GLFWwindow;

namespace vox {

struct WindowConfig {
    std::string title = "Vox";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool vsync = true;
};

// Owns the OS window and the OpenGL context. The first window created loads
// the GL function pointers; the engine currently assumes a single window.
class Window {
public:
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void PollEvents();
    void SwapBuffers();

    bool ShouldClose() const;
    void RequestClose();

    void SetVsync(bool enabled);
    bool IsVsync() const { return m_config.vsync; }

    // Captured = cursor hidden and locked to the window (mouselook mode).
    void SetCursorCaptured(bool captured);
    bool IsCursorCaptured() const { return m_cursorCaptured; }

    uint32_t Width() const { return m_config.width; }
    uint32_t Height() const { return m_config.height; }

    void SetTitle(const std::string& title);
    void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }

    GLFWwindow* Handle() const { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
    WindowConfig m_config;
    ResizeCallback m_resizeCallback;
    bool m_cursorCaptured = false;
};

} // namespace vox
