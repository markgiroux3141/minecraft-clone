#include "vox/renderer/TextureArray.h"

#include <bit>
#include <stdexcept>
#include <vector>

#include <glad/gl.h>
#include <stb_image.h>

#include "vox/core/Assert.h"
#include "vox/core/Assets.h"
#include "vox/core/Log.h"

namespace vox {

Texture2DArray::Texture2DArray(uint32_t tileSize, uint32_t layers, bool pixelArtFilter)
    : m_tileSize(tileSize), m_layers(layers) {
    const auto mipLevels = static_cast<GLsizei>(std::bit_width(tileSize));

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_handle);
    glTextureStorage3D(m_handle, mipLevels, GL_RGBA8, static_cast<GLsizei>(tileSize),
                       static_cast<GLsizei>(tileSize), static_cast<GLsizei>(layers));

    const GLint minFilter = pixelArtFilter ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
    const GLint magFilter = pixelArtFilter ? GL_NEAREST : GL_LINEAR;
    glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, minFilter);
    glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, magFilter);
    // REPEAT within a layer lets greedy-meshed quads tile their texture.
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Texture2DArray::~Texture2DArray() {
    glDeleteTextures(1, &m_handle);
}

std::shared_ptr<Texture2DArray> Texture2DArray::FromFileStrip(std::string_view path,
                                                              uint32_t tileSize,
                                                              bool pixelArtFilter) {
    const auto fullPath = assets::Resolve(path).string();

    // Tiles are flipped individually below, which works for any grid layout
    // (a whole-image flip would also reorder tile rows).
    stbi_set_flip_vertically_on_load(0);
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture strip '" + fullPath +
                                 "': " + stbi_failure_reason());
    }
    if (width % tileSize != 0 || height % tileSize != 0) {
        stbi_image_free(pixels);
        throw std::runtime_error("Texture strip '" + fullPath +
                                 "' is not a multiple of the tile size");
    }

    const uint32_t cols = static_cast<uint32_t>(width) / tileSize;
    const uint32_t rows = static_cast<uint32_t>(height) / tileSize;
    auto array = std::make_shared<Texture2DArray>(tileSize, cols * rows, pixelArtFilter);

    const size_t rowBytes = static_cast<size_t>(tileSize) * 4;
    std::vector<stbi_uc> tile(rowBytes * tileSize);
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            for (uint32_t y = 0; y < tileSize; ++y) {
                // Source row from the top, written bottom-first (GL origin).
                const stbi_uc* src = pixels +
                                     ((static_cast<size_t>(row) * tileSize + y) * width +
                                      static_cast<size_t>(col) * tileSize) *
                                         4;
                std::copy_n(src, rowBytes, tile.data() + (tileSize - 1 - y) * rowBytes);
            }
            array->SetLayerData(row * cols + col, tile.data());
        }
    }
    stbi_image_free(pixels);
    array->GenerateMipmaps();

    VOX_TRACE("Loaded texture array '{}' ({} layers of {}x{})", fullPath, cols * rows, tileSize,
              tileSize);
    return array;
}

void Texture2DArray::SetLayerData(uint32_t layer, const void* rgba8Pixels) {
    VOX_ASSERT(layer < m_layers, "Texture array layer out of range");
    glTextureSubImage3D(m_handle, 0, 0, 0, static_cast<GLint>(layer),
                        static_cast<GLsizei>(m_tileSize), static_cast<GLsizei>(m_tileSize), 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, rgba8Pixels);
}

void Texture2DArray::GenerateMipmaps() {
    glGenerateTextureMipmap(m_handle);
}

void Texture2DArray::Bind(uint32_t slot) const {
    glBindTextureUnit(slot, m_handle);
}

} // namespace vox
