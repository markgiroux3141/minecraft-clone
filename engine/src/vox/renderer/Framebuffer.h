#pragma once

#include <cstdint>
#include <memory>

namespace vox {

class Texture2D;

// Offscreen render target: one RGBA8 color texture + a depth renderbuffer.
// Used to bake content once into a texture that the rest of the pipeline then
// samples (e.g. the 3D block-icon sheet drawn into the 2D HUD). Bind() makes
// it the active framebuffer and sets the viewport to its size; restore the
// default framebuffer with Unbind() and reset the viewport afterwards.
class Framebuffer {
public:
    Framebuffer(uint32_t width, uint32_t height);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void Bind() const;
    // Rebinds the window's default framebuffer (0). The caller is responsible
    // for restoring its own viewport (Renderer::SetViewport).
    static void Unbind();

    const std::shared_ptr<Texture2D>& Color() const { return m_color; }
    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

private:
    uint32_t m_handle = 0;       // FBO
    uint32_t m_depth = 0;        // depth renderbuffer
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::shared_ptr<Texture2D> m_color;
};

} // namespace vox
