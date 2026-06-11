#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace vox {

class Texture2D {
public:
    // Creates an RGBA8 texture from raw pixel data (tightly packed, bottom row first).
    Texture2D(uint32_t width, uint32_t height, const void* rgba8Pixels,
              bool pixelArtFilter = true);
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    // Loads from an asset-relative path, e.g. "textures/test_block.png".
    static std::shared_ptr<Texture2D> FromFile(std::string_view path, bool pixelArtFilter = true);

    void Bind(uint32_t slot = 0) const;

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

private:
    uint32_t m_handle = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace vox
