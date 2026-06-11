#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace vox {

// Two channels: the engine logs through VOX_* macros, game code through GAME_*.
// Keeping them separate makes it obvious which layer a message came from.
class Log {
public:
    static void Init();

    static spdlog::logger& Engine() { return *s_engine; }
    static spdlog::logger& Game() { return *s_game; }

private:
    static std::shared_ptr<spdlog::logger> s_engine;
    static std::shared_ptr<spdlog::logger> s_game;
};

} // namespace vox

#define VOX_TRACE(...) ::vox::Log::Engine().trace(__VA_ARGS__)
#define VOX_INFO(...) ::vox::Log::Engine().info(__VA_ARGS__)
#define VOX_WARN(...) ::vox::Log::Engine().warn(__VA_ARGS__)
#define VOX_ERROR(...) ::vox::Log::Engine().error(__VA_ARGS__)
#define VOX_CRITICAL(...) ::vox::Log::Engine().critical(__VA_ARGS__)

#define GAME_TRACE(...) ::vox::Log::Game().trace(__VA_ARGS__)
#define GAME_INFO(...) ::vox::Log::Game().info(__VA_ARGS__)
#define GAME_WARN(...) ::vox::Log::Game().warn(__VA_ARGS__)
#define GAME_ERROR(...) ::vox::Log::Game().error(__VA_ARGS__)
