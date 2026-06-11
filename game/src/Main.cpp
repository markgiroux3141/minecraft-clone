#include <cstdio>
#include <exception>

#include "GameApp.h"

int main() {
    try {
        GameApp app;
        app.Run();
    } catch (const std::exception& e) {
        // The logger may not exist if startup failed, so use stderr directly.
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    }
    return 0;
}
