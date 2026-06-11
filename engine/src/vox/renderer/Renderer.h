#pragma once

#include <cstdint>

namespace vox {

class VertexArray;

// Thin facade over the OpenGL backend. As the engine grows this becomes the
// seam where a renderer abstraction (command lists, future Vulkan backend)
// slots in; game code should never call GL directly.
class Renderer {
public:
    static void Init();
    static void Shutdown();

    static void SetClearColor(float r, float g, float b, float a = 1.0f);
    static void Clear();
    static void SetViewport(uint32_t width, uint32_t height);

    // Draws indexCount indices from the bound-on-demand vertex array;
    // 0 means "the whole index buffer".
    static void DrawIndexed(const VertexArray& vertexArray, uint32_t indexCount = 0);

    // Same, but as line segments (index pairs) — debug/UI overlays.
    static void DrawLines(const VertexArray& vertexArray, uint32_t indexCount = 0);
};

} // namespace vox
