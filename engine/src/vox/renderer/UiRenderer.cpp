#include "vox/renderer/UiRenderer.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assert.h"
#include "vox/renderer/Buffer.h"
#include "vox/renderer/Renderer.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"
#include "vox/renderer/TextureArray.h"
#include "vox/renderer/VertexArray.h"

namespace vox {

namespace {

constexpr uint32_t kMaxQuads = 4096;

constexpr float kModeSolid = 0.0f;
constexpr float kModeFont = 1.0f;
constexpr float kModeAtlas = 2.0f;
constexpr float kModeImage = 3.0f;

// The vertex layout and sampler contract live with the batcher, so the shader
// is embedded rather than shipped as an asset the game could drift from.
constexpr std::string_view kVertexSrc = R"(#version 460 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec3 a_uvw;
layout(location = 2) in float a_mode;
layout(location = 3) in vec4 a_color;

uniform mat4 u_proj;

out vec3 v_uvw;
flat out float v_mode;
out vec4 v_color;

void main() {
    v_uvw = a_uvw;
    v_mode = a_mode;
    v_color = a_color;
    gl_Position = u_proj * vec4(a_position, 0.0, 1.0);
}
)";

// textureLod: UI always magnifies (level 0), and explicit lod keeps the
// samplers out of derivative trouble inside non-uniform branches.
constexpr std::string_view kFragmentSrc = R"(#version 460 core
in vec3 v_uvw;
flat in float v_mode;
in vec4 v_color;

uniform sampler2D u_font;
uniform sampler2DArray u_atlas;
uniform sampler2D u_image;

out vec4 o_color;

void main() {
    vec4 color = v_color;
    int mode = int(round(v_mode));
    if (mode == 1) {
        color *= textureLod(u_font, v_uvw.xy, 0.0);
    } else if (mode == 2) {
        color *= textureLod(u_atlas, v_uvw, 0.0);
    } else if (mode == 3) {
        color *= textureLod(u_image, v_uvw.xy, 0.0);
    }
    o_color = color;
}
)";

} // namespace

UiRenderer::UiRenderer() {
    m_shader = std::make_unique<Shader>("ui", kVertexSrc, kFragmentSrc);
    m_shader->Bind();
    m_shader->SetInt("u_font", 0);
    m_shader->SetInt("u_atlas", 1);
    m_shader->SetInt("u_image", 2);

    m_vertexBuffer =
        std::make_shared<VertexBuffer>(static_cast<uint32_t>(kMaxQuads * 4 * sizeof(Vertex)));
    m_vertexBuffer->SetLayout({
        {ShaderDataType::Float2, "a_position"},
        {ShaderDataType::Float3, "a_uvw"},
        {ShaderDataType::Float, "a_mode"},
        {ShaderDataType::Float4, "a_color"},
    });

    // One shared {0,1,2, 2,3,0} pattern per quad, built once.
    std::vector<uint32_t> indices(kMaxQuads * 6);
    for (uint32_t quad = 0; quad < kMaxQuads; ++quad) {
        const uint32_t base = quad * 4;
        uint32_t* out = indices.data() + static_cast<size_t>(quad) * 6;
        out[0] = base + 0;
        out[1] = base + 1;
        out[2] = base + 2;
        out[3] = base + 2;
        out[4] = base + 3;
        out[5] = base + 0;
    }

    m_vertexArray = std::make_shared<VertexArray>();
    m_vertexArray->AddVertexBuffer(m_vertexBuffer);
    m_vertexArray->SetIndexBuffer(
        std::make_shared<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size())));

    m_vertices.reserve(kMaxQuads * 4);
}

UiRenderer::~UiRenderer() = default;

void UiRenderer::SetFont(std::shared_ptr<Texture2D> texture, uint32_t columns, uint32_t rows,
                         char firstChar, bool proportional) {
    VOX_ASSERT(columns > 0 && rows > 0, "font grid must be non-empty");
    m_font = std::move(texture);
    m_fontColumns = columns;
    m_fontRows = rows;
    m_fontFirstChar = firstChar;
    m_glyphSize = {static_cast<float>(m_font->Width() / columns),
                   static_cast<float>(m_font->Height() / rows)};

    m_advances.assign(static_cast<size_t>(columns) * rows, m_glyphSize.x);
    if (!proportional) {
        return;
    }

    // Scan each cell's alpha for its rightmost inked column (glyphs sit at
    // the cell's left edge). The texture was uploaded bottom-row-first, but
    // column extents don't care about vertical order — only the cell-row
    // index needs flipping.
    const uint32_t width = m_font->Width();
    const uint32_t height = m_font->Height();
    const auto cellW = static_cast<uint32_t>(m_glyphSize.x);
    const auto cellH = static_cast<uint32_t>(m_glyphSize.y);
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    glGetTextureImage(m_font->RendererId(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      static_cast<GLsizei>(pixels.size()), pixels.data());

    for (uint32_t row = 0; row < m_fontRows; ++row) {
        for (uint32_t col = 0; col < m_fontColumns; ++col) {
            const uint32_t x0 = col * cellW;
            const uint32_t y0 = height - (row + 1) * cellH; // flipped rows
            int inked = -1;
            for (int x = static_cast<int>(cellW) - 1; x >= 0 && inked < 0; --x) {
                for (uint32_t y = 0; y < cellH; ++y) {
                    const size_t at =
                        (static_cast<size_t>(y0 + y) * width + x0 + static_cast<uint32_t>(x)) * 4;
                    if (pixels[at + 3] != 0) {
                        inked = x;
                        break;
                    }
                }
            }
            // Inked glyphs get a 1px gap; empty cells (space) half a cell.
            m_advances[static_cast<size_t>(row) * m_fontColumns + col] =
                inked >= 0 ? static_cast<float>(inked + 2) : m_glyphSize.x * 0.5f;
        }
    }
}

void UiRenderer::Begin(uint32_t screenWidth, uint32_t screenHeight, const Texture2DArray* atlas) {
    m_projection = glm::ortho(0.0f, static_cast<float>(screenWidth),
                              static_cast<float>(screenHeight), 0.0f, -1.0f, 1.0f);
    m_atlas = atlas;
    m_vertices.clear();
}

void UiRenderer::DrawRect(glm::vec2 pos, glm::vec2 size, glm::vec4 color) {
    PushQuad(pos, size, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, kModeSolid, color);
}

void UiRenderer::DrawAtlasTile(glm::vec2 pos, glm::vec2 size, uint16_t layer, glm::vec4 tint) {
    VOX_ASSERT(m_atlas, "DrawAtlasTile needs an atlas passed to Begin");
    const auto w = static_cast<float>(layer);
    // Textures load flipped for GL's bottom-left origin: image-top is v=1.
    PushQuad(pos, size, {0.0f, 1.0f, w}, {1.0f, 0.0f, w}, kModeAtlas, tint);
}

void UiRenderer::DrawImage(std::shared_ptr<Texture2D> texture, glm::vec2 pos, glm::vec2 size,
                           glm::vec2 srcPos, glm::vec2 srcSize, glm::vec4 tint) {
    VOX_ASSERT(texture, "DrawImage needs a texture");
    if (m_image && m_image.get() != texture.get()) {
        Flush(); // one image sheet per batch
    }
    m_image = std::move(texture);
    const auto w = static_cast<float>(m_image->Width());
    const auto h = static_cast<float>(m_image->Height());
    // Textures load flipped for GL's bottom-left origin: image-top is v=1.
    PushQuad(pos, size, {srcPos.x / w, 1.0f - srcPos.y / h, 0.0f},
             {(srcPos.x + srcSize.x) / w, 1.0f - (srcPos.y + srcSize.y) / h, 0.0f}, kModeImage,
             tint);
}

void UiRenderer::DrawText(glm::vec2 pos, std::string_view text, float scale, glm::vec4 color) {
    VOX_ASSERT(m_font, "DrawText needs a font (SetFont)");
    const glm::vec2 glyph = m_glyphSize * scale;
    glm::vec2 pen = pos;
    for (const char c : text) {
        if (c == '\n') {
            pen.x = pos.x;
            pen.y += glyph.y;
            continue;
        }
        const int index = c - m_fontFirstChar;
        if (index < 0 || index >= static_cast<int>(m_fontColumns * m_fontRows)) {
            pen.x += glyph.x;
            continue;
        }
        if (c != ' ') {
            const auto col = static_cast<float>(index % static_cast<int>(m_fontColumns));
            const auto row = static_cast<float>(index / static_cast<int>(m_fontColumns));
            const float u0 = col / static_cast<float>(m_fontColumns);
            const float u1 = (col + 1.0f) / static_cast<float>(m_fontColumns);
            const float vTop = 1.0f - row / static_cast<float>(m_fontRows);
            const float vBottom = 1.0f - (row + 1.0f) / static_cast<float>(m_fontRows);
            PushQuad(pen, glyph, {u0, vTop, 0.0f}, {u1, vBottom, 0.0f}, kModeFont, color);
        }
        pen.x += m_advances[static_cast<size_t>(index)] * scale;
    }
}

glm::vec2 UiRenderer::MeasureText(std::string_view text, float scale) const {
    float maxLine = 0.0f;
    float line = 0.0f;
    size_t lines = 1;
    for (const char c : text) {
        if (c == '\n') {
            ++lines;
            line = 0.0f;
            continue;
        }
        const int index = c - m_fontFirstChar;
        line += (index >= 0 && index < static_cast<int>(m_advances.size()))
                    ? m_advances[static_cast<size_t>(index)]
                    : m_glyphSize.x;
        maxLine = std::max(maxLine, line);
    }
    return {maxLine * scale, m_glyphSize.y * scale * static_cast<float>(lines)};
}

void UiRenderer::End() {
    Flush();
    m_atlas = nullptr;
    m_image.reset();
}

void UiRenderer::PushQuad(glm::vec2 pos, glm::vec2 size, glm::vec3 uvwMin, glm::vec3 uvwMax,
                          float mode, glm::vec4 color) {
    if (m_vertices.size() >= kMaxQuads * 4) {
        Flush();
    }
    m_vertices.push_back({pos, uvwMin, mode, color});
    m_vertices.push_back({{pos.x + size.x, pos.y}, {uvwMax.x, uvwMin.y, uvwMin.z}, mode, color});
    m_vertices.push_back({pos + size, uvwMax, mode, color});
    m_vertices.push_back({{pos.x, pos.y + size.y}, {uvwMin.x, uvwMax.y, uvwMin.z}, mode, color});
}

void UiRenderer::Flush() {
    if (m_vertices.empty()) {
        return;
    }

    m_vertexBuffer->SetData(m_vertices.data(),
                            static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));

    m_shader->Bind();
    m_shader->SetMat4("u_proj", m_projection);
    if (m_font) {
        m_font->Bind(0);
    }
    if (m_atlas) {
        m_atlas->Bind(1);
    }
    if (m_image) {
        m_image->Bind(2);
    }

    // Overlay pass: no depth, no culling (quads wind whichever way), blended.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto quadCount = static_cast<uint32_t>(m_vertices.size() / 4);
    Renderer::DrawIndexed(*m_vertexArray, quadCount * 6);

    // Restore the 3D pass's baseline state (see Renderer::Init).
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    m_vertices.clear();
}

} // namespace vox
