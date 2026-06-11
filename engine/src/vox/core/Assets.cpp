#include "vox/core/Assets.h"

#include <fstream>
#include <stdexcept>

#include "vox/core/Log.h"

#ifdef _WIN32
// WIN32_LEAN_AND_MEAN and NOMINMAX come from the build settings.
#include <windows.h>
#endif

namespace vox::assets {

namespace {

std::filesystem::path ExecutableDir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path LocateRoot() {
    std::filesystem::path candidates[] = {std::filesystem::current_path(), ExecutableDir()};
    for (auto base : candidates) {
        // Walk upward a few levels so build/<config>/bin finds the repo root.
        for (int i = 0; i < 5; ++i) {
            if (auto dir = base / "assets"; std::filesystem::is_directory(dir)) {
                return dir;
            }
            if (!base.has_parent_path() || base.parent_path() == base) {
                break;
            }
            base = base.parent_path();
        }
    }
    throw std::runtime_error("Could not locate the assets/ directory");
}

} // namespace

const std::filesystem::path& Root() {
    static const std::filesystem::path root = [] {
        auto path = LocateRoot();
        VOX_INFO("Asset root: {}", path.string());
        return path;
    }();
    return root;
}

std::filesystem::path Resolve(std::string_view relative) {
    return Root() / relative;
}

std::string ReadText(std::string_view relative) {
    const auto path = Resolve(relative);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open asset file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace vox::assets
