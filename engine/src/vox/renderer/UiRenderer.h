#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace vox {

class Shader;
class Texture2D;
class Texture2DArray;
class VertexArray;
class VertexBuffer;

// Batched 2D overlay renderer for HUD/menus: solid rects, monospace bitmap
// text, and single layers of a texture array (block icons). Everything pushed
// between Begin and End goes out in one draw call (auto-flushing if the batch
// fills). Coordinates are framebuffer pixels with the origin at the top-left;
// the pass draws with depth testing off and alpha blending on.
class UiRenderer {
public:
    UiRenderer();
    ~UiRenderer();

    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;

    // Bitmap font: a grid of equal cells in ASCII order starting at firstChar,
    // row-major from the image's top-left. Glyph size is derived from the
    // texture dimensions and the grid. With proportional=true, per-glyph
    // advances are scanned from the texture's alpha (Minecraft-style:
    // rightmost inked column + 1px gap; empty cells advance half a cell);
    // otherwise every glyph advances one full cell (monospace).
    void SetFont(std::shared_ptr<Texture2D> texture, uint32_t columns, uint32_t rows,
                 char firstChar = ' ', bool proportional = false);
    glm::vec2 GlyphSize() const { return m_glyphSize; }

    // atlas may be null when no DrawAtlasTile calls will be made this frame.
    void Begin(uint32_t screenWidth, uint32_t screenHeight,
               const Texture2DArray* atlas = nullptr);
    void DrawRect(glm::vec2 pos, glm::vec2 size, glm::vec4 color);
    // One layer of the atlas passed to Begin, stretched over the rect.
    void DrawAtlasTile(glm::vec2 pos, glm::vec2 size, uint16_t layer,
                       glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f});
    // A sub-rectangle of an arbitrary texture (sprite sheets: GUI widgets,
    // icons). srcPos/srcSize are in image pixels with the origin at the
    // image's top-left. Switching textures mid-batch forces a flush, so
    // group draws from the same sheet together where convenient.
    void DrawImage(std::shared_ptr<Texture2D> texture, glm::vec2 pos, glm::vec2 size,
                   glm::vec2 srcPos, glm::vec2 srcSize,
                   glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f});
    // scale multiplies the glyph cell; integer scales stay pixel-crisp.
    // Understands '\n'. Characters outside the font's range are skipped.
    void DrawText(glm::vec2 pos, std::string_view text, float scale, glm::vec4 color);
    glm::vec2 MeasureText(std::string_view text, float scale) const;
    void End();

private:
    struct Vertex {
        glm::vec2 pos;
        glm::vec3 uvw; // uv + atlas layer
        float mode;    // kModeSolid/kModeFont/kModeAtlas
        glm::vec4 color;
    };

    void PushQuad(glm::vec2 pos, glm::vec2 size, glm::vec3 uvwMin, glm::vec3 uvwMax, float mode,
                  glm::vec4 color);
    void Flush();

    std::unique_ptr<Shader> m_shader;
    std::shared_ptr<VertexArray> m_vertexArray;
    std::shared_ptr<VertexBuffer> m_vertexBuffer;
    std::vector<Vertex> m_vertices;

    std::shared_ptr<Texture2D> m_font;
    uint32_t m_fontColumns = 0;
    uint32_t m_fontRows = 0;
    char m_fontFirstChar = ' ';
    glm::vec2 m_glyphSize{0.0f};
    std::vector<float> m_advances; // per glyph, in unscaled pixels

    std::shared_ptr<Texture2D> m_image; // current DrawImage sheet
    const Texture2DArray* m_atlas = nullptr;
    glm::mat4 m_projection{1.0f};
};

} // namespace vox
