#include "vox/core/Application.h"

#include <algorithm>
#include <chrono>

#include "vox/core/Assert.h"
#include "vox/core/Log.h"
#include "vox/platform/Input.h"
#include "vox/renderer/Renderer.h"

namespace vox {

Application* Application::s_instance = nullptr;

Application::Application(const ApplicationConfig& config) : m_config(config) {
    Log::Init();
    VOX_ASSERT(!s_instance, "Only one Application may exist");
    s_instance = this;

    m_window = std::make_unique<Window>(m_config.window);
    Input::Attach(*m_window);
    Renderer::Init();

    m_window->SetResizeCallback([this](uint32_t width, uint32_t height) {
        Renderer::SetViewport(width, height);
        OnResize(width, height);
    });
}

Application::~Application() {
    Renderer::Shutdown();
    m_window.reset();
    s_instance = nullptr;
}

Application& Application::Get() {
    VOX_ASSERT(s_instance, "Application not created yet");
    return *s_instance;
}

void Application::Run() {
    using Clock = std::chrono::steady_clock;

    OnInit();

    const double tickDt = 1.0 / m_config.tickRate;
    double accumulator = 0.0;
    auto lastTime = Clock::now();

    while (m_running && !m_window->ShouldClose()) {
        const auto now = Clock::now();
        double frameDt = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;

        // Cap the catch-up work after stalls (debugger pauses, window drags)
        // so the simulation can't spiral.
        frameDt = std::min(frameDt, 0.25);
        accumulator += frameDt;

        m_window->PollEvents();

        while (accumulator >= tickDt) {
            OnTick(tickDt);
            accumulator -= tickDt;
        }

        OnRender(accumulator / tickDt, frameDt);
        m_window->SwapBuffers();
    }

    OnShutdown();
}

void Application::Close() {
    m_running = false;
}

} // namespace vox
