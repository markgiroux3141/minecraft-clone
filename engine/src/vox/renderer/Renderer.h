#pragma once

#include <cstdint>

namespace vox {

class VertexArray;

// Thin facade over the OpenGL backend. As the engine grows this becomes the
// seam where a renderer abstraction (command lists, future Vulkan backend)
// slots in; game code should never call GL directly.
// Alpha = classic src-alpha blending (water); Additive adds onto the
// framebuffer (sun/moon — black pixels contribute nothing); Crumble is
// Minecraft's block-damage blend, out = 2 * src * dst — mid-gray neutral,
// dark pixels darken, light pixels highlight.
enum class BlendMode { None, Alpha, Additive, Crumble };

class Renderer {
public:
    static void Init();
    static void Shutdown();

    static void SetClearColor(float r, float g, float b, float a = 1.0f);
    static void Clear();
    // Depth only — first-person view-model pass (the hand draws over the
    // world and must never clip into walls).
    static void ClearDepth();
    static void SetViewport(uint32_t width, uint32_t height);

    // Pass-level state for blended geometry (water). Baseline state is
    // blend off, depth write on, culling on — restore after the pass.
    static void SetBlend(BlendMode mode);
    static void SetDepthWrite(bool enabled);
    static void SetCullFace(bool enabled);

    // Draws indexCount indices from the bound-on-demand vertex array;
    // 0 means "the whole index buffer".
    static void DrawIndexed(const VertexArray& vertexArray, uint32_t indexCount = 0);

    // Same, but as line segments (index pairs) — debug/UI overlays.
    static void DrawLines(const VertexArray& vertexArray, uint32_t indexCount = 0);
};

} // namespace vox
