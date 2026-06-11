#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace vox::assets {

// Root of the assets/ directory, located once at startup. Searched relative
// to the working directory first, then the executable (and its parents, so
// running straight from build/<config>/bin works).
const std::filesystem::path& Root();

// Resolve a path like "shaders/cube.vert" against the asset root.
std::filesystem::path Resolve(std::string_view relative);

// Read an entire asset text file; throws std::runtime_error with the path on failure.
std::string ReadText(std::string_view relative);

} // namespace vox::assets
