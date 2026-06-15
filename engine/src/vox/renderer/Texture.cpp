#include "vox/renderer/Texture.h"

#include <algorithm>
#include <bit>
#include <stdexcept>

#include <glad/gl.h>
#include <stb_image.h>

#include "vox/core/Assets.h"
#include "vox/core/Log.h"

namespace vox {

Texture2D::Texture2D(uint32_t width, uint32_t height, const void* rgba8Pixels,
                     bool pixelArtFilter)
    : m_width(width), m_height(height) {
    const auto mipLevels =
        static_cast<GLsizei>(std::bit_width(std::max(width, height)));

    glCreateTextures(GL_TEXTURE_2D, 1, &m_handle);
    glTextureStorage2D(m_handle, mipLevels, GL_RGBA8, static_cast<GLsizei>(width),
                       static_cast<GLsizei>(height));
    glTextureSubImage2D(m_handle, 0, 0, 0, static_cast<GLsizei>(width),
                        static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, rgba8Pixels);
    glGenerateTextureMipmap(m_handle);

    // Pixel-art (Minecraft) look: crisp nearest magnification, mipmapped
    // minification to avoid shimmer at a distance.
    const GLint minFilter = pixelArtFilter ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
    const GLint magFilter = pixelArtFilter ? GL_NEAREST : GL_LINEAR;
    glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, minFilter);
    glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, magFilter);
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Texture2D::Texture2D(uint32_t width, uint32_t height) : m_width(width), m_height(height) {
    glCreateTextures(GL_TEXTURE_2D, 1, &m_handle);
    glTextureStorage2D(m_handle, 1, GL_RGBA8, static_cast<GLsizei>(width),
                       static_cast<GLsizei>(height));
    // Crisp 1:1 sampling for baked icon sheets; clamp so edge cells never
    // bleed into one another.
    glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

Texture2D::~Texture2D() {
    glDeleteTextures(1, &m_handle);
}

std::shared_ptr<Texture2D> Texture2D::FromFile(std::string_view path, bool pixelArtFilter) {
    const auto fullPath = assets::Resolve(path).string();

    stbi_set_flip_vertically_on_load(1); // match GL's bottom-left UV origin
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture '" + fullPath +
                                 "': " + stbi_failure_reason());
    }

    auto texture = std::make_shared<Texture2D>(static_cast<uint32_t>(width),
                                               static_cast<uint32_t>(height), pixels,
                                               pixelArtFilter);
    stbi_image_free(pixels);

    VOX_TRACE("Loaded texture '{}' ({}x{})", fullPath, width, height);
    return texture;
}

void Texture2D::Bind(uint32_t slot) const {
    glBindTextureUnit(slot, m_handle);
}

} // namespace vox
