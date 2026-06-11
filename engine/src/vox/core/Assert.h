#pragma once

#include "vox/core/Log.h"

#ifdef VOX_DEBUG
#if defined(_MSC_VER)
#define VOX_DEBUGBREAK() __debugbreak()
#else
#include <csignal>
#define VOX_DEBUGBREAK() raise(SIGTRAP)
#endif

#define VOX_ASSERT(condition, message)                                                             \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            VOX_CRITICAL("Assertion failed: {} ({}:{})", message, __FILE__, __LINE__);             \
            VOX_DEBUGBREAK();                                                                      \
        }                                                                                          \
    } while (false)
#else
#define VOX_ASSERT(condition, message) ((void)0)
#endif
