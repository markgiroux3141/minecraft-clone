#include "vox/core/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace vox {

std::shared_ptr<spdlog::logger> Log::s_engine;
std::shared_ptr<spdlog::logger> Log::s_game;

void Log::Init() {
    if (s_engine) {
        return;
    }

    s_engine = spdlog::stdout_color_mt("VOX");
    s_game = spdlog::stdout_color_mt("GAME");

    for (auto& logger : {s_engine, s_game}) {
        logger->set_pattern("%^[%T.%e] [%n] %v%$");
#ifdef VOX_DEBUG
        logger->set_level(spdlog::level::trace);
#else
        logger->set_level(spdlog::level::info);
#endif
    }
}

} // namespace vox
