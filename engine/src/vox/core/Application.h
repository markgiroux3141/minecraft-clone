#pragma once

#include <memory>

#include "vox/platform/Window.h"

namespace vox {

struct ApplicationConfig {
    WindowConfig window;
    // Fixed simulation rate, decoupled from the render framerate
    // (Minecraft simulates at 20 ticks per second).
    double tickRate = 20.0;
};

// Owns the main loop: fixed-timestep simulation with an interpolated,
// uncapped render pass. Games subclass this and override the hooks.
class Application {
public:
    explicit Application(const ApplicationConfig& config);
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run();
    void Close();

    Window& GetWindow() { return *m_window; }
    static Application& Get();

protected:
    // Called once after the window and renderer exist.
    virtual void OnInit() {}
    // Fixed-timestep simulation step; dt is always 1/tickRate seconds.
    virtual void OnTick(double dt) { (void)dt; }
    // Per-frame render. alpha in [0,1) is the fraction of a tick elapsed,
    // for interpolating between the previous and current simulation state.
    virtual void OnRender(double alpha, double frameDt) {
        (void)alpha;
        (void)frameDt;
    }
    // Framebuffer size changed (already applied to the GL viewport).
    virtual void OnResize(uint32_t width, uint32_t height) {
        (void)width;
        (void)height;
    }
    virtual void OnShutdown() {}

private:
    ApplicationConfig m_config;
    std::unique_ptr<Window> m_window;
    bool m_running = true;

    static Application* s_instance;
};

} // namespace vox
