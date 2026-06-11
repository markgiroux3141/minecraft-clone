#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace vox {

// 2D texture array of square, equally-sized tiles — the natural fit for block
// textures: no atlas bleeding across mip levels, and UVs can tile within a
// layer (required by greedy meshing).
class Texture2DArray {
public:
    Texture2DArray(uint32_t tileSize, uint32_t layers, bool pixelArtFilter = true);
    ~Texture2DArray();

    Texture2DArray(const Texture2DArray&) = delete;
    Texture2DArray& operator=(const Texture2DArray&) = delete;

    // Loads a strip/grid PNG of square tiles (asset-relative path); tiles are
    // assigned layer indices left-to-right, top-to-bottom.
    static std::shared_ptr<Texture2DArray> FromFileStrip(std::string_view path,
                                                         uint32_t tileSize,
                                                         bool pixelArtFilter = true);

    // rgba8Pixels: tileSize x tileSize, tightly packed, bottom row first.
    void SetLayerData(uint32_t layer, const void* rgba8Pixels);
    void GenerateMipmaps();

    void Bind(uint32_t slot = 0) const;

    uint32_t TileSize() const { return m_tileSize; }
    uint32_t Layers() const { return m_layers; }

private:
    uint32_t m_handle = 0;
    uint32_t m_tileSize = 0;
    uint32_t m_layers = 0;
};

} // namespace vox
