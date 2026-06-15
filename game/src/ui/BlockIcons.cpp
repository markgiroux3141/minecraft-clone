#include "ui/BlockIcons.h"

#include <array>
#include <cmath>
#include <format>

#include <glm/gtc/matrix_transform.hpp>

#include "vox/renderer/Buffer.h"
#include "vox/renderer/Framebuffer.h"
#include "vox/renderer/Renderer.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"
#include "vox/renderer/TextureArray.h"
#include "vox/renderer/VertexArray.h"

#include "world/Block.h"

namespace vc {

namespace {

constexpr int kColumns = 12;

// Per-face slicing basis, mirroring ChunkMesher's kFaces (BlockFace order) so
// icon textures read identically to the world: (uAxis, vAxis) span the face,
// swapUv keeps texture-up aligned, n/s are the normal axis and its sign.
struct FaceBasis {
    int n, s, uAxis, vAxis;
    bool swapUv;
};
constexpr FaceBasis kFaces[6] = {
    {0, +1, 1, 2, true},  // +X
    {0, -1, 2, 1, false}, // -X
    {1, +1, 2, 0, false}, // +Y
    {1, -1, 0, 2, false}, // -Y
    {2, +1, 0, 1, false}, // +Z
    {2, -1, 1, 0, true},  // -Z
};

// Append one axis-aligned box (corners in 0..1 block space) to a vertex/index
// buffer in the cube vertex layout (pos, normal, uv, face). UVs sample the
// slice of each face's tile that the box occupies — same rule as the mesher's
// ShapeBox, so a half-height slab side shows the tile's lower half.
void EmitBox(std::vector<float>& verts, std::vector<uint32_t>& indices, const glm::vec3& from,
             const glm::vec3& to) {
    constexpr int cu[4] = {0, 1, 1, 0};
    constexpr int cv[4] = {0, 0, 1, 1};
    const float lo[3] = {from.x, from.y, from.z};
    const float hi[3] = {to.x, to.y, to.z};

    for (int f = 0; f < 6; ++f) {
        const FaceBasis& fb = kFaces[f];
        const auto base = static_cast<uint32_t>(verts.size() / 9);
        for (int k = 0; k < 4; ++k) {
            float pos[3];
            pos[fb.n] = fb.s > 0 ? hi[fb.n] : lo[fb.n];
            pos[fb.uAxis] = cu[k] ? hi[fb.uAxis] : lo[fb.uAxis];
            pos[fb.vAxis] = cv[k] ? hi[fb.vAxis] : lo[fb.vAxis];
            const float tu = cu[k] ? hi[fb.uAxis] : lo[fb.uAxis];
            const float tv = cv[k] ? hi[fb.vAxis] : lo[fb.vAxis];
            float normal[3] = {0.0f, 0.0f, 0.0f};
            normal[fb.n] = static_cast<float>(fb.s);
            verts.insert(verts.end(), {pos[0], pos[1], pos[2], normal[0], normal[1], normal[2],
                                       fb.swapUv ? tv : tu, fb.swapUv ? tu : tv,
                                       static_cast<float>(f)});
        }
        indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }
}

std::shared_ptr<vox::VertexArray> MakeMesh(const std::vector<float>& verts,
                                           const std::vector<uint32_t>& indices) {
    auto buffer = std::make_shared<vox::VertexBuffer>(
        verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float)));
    buffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                       {vox::ShaderDataType::Float3, "a_normal"},
                       {vox::ShaderDataType::Float2, "a_uv"},
                       {vox::ShaderDataType::Float, "a_face"}});
    auto vao = std::make_shared<vox::VertexArray>();
    vao->AddVertexBuffer(std::move(buffer));
    vao->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
        indices.data(), static_cast<uint32_t>(indices.size())));
    return vao;
}

} // namespace

bool BlockIcons::Has3dIcon(ItemId id) {
    if (!IsBlockItem(id) || id == blocks::Air) {
        return false;
    }
    if (RenderAsSprite(id)) {
        return false; // plants, torches: flat sprite like vanilla
    }
    // Liquid flow ids (level 1..7) never appear as items; only the source (8)
    // or non-liquid (0) blocks get an icon.
    const uint8_t level = BlockRegistry::Get().Def(id).liquidLevel;
    return level == 0 || level == 8;
}

BlockIcons::BlockIcons(std::shared_ptr<vox::Texture2DArray> atlas) : m_atlas(std::move(atlas)) {
    m_shader = vox::Shader::FromFiles("shaders/block_icon.vert", "shaders/block_icon.frag");

    std::vector<float> verts;
    std::vector<uint32_t> indices;

    EmitBox(verts, indices, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    m_cube = MakeMesh(verts, indices);

    verts.clear();
    indices.clear();
    EmitBox(verts, indices, {0.0f, 0.0f, 0.0f}, {1.0f, 0.5f, 1.0f});
    m_slab = MakeMesh(verts, indices);

    verts.clear();
    indices.clear();
    // Straight stair, canonical facing: a bottom half-slab plus a quarter step
    // on the +Z half. Under the 225° GUI yaw that puts the tall riser at the
    // BACK and the step facing the viewer (the vanilla inventory look).
    EmitBox(verts, indices, {0.0f, 0.0f, 0.0f}, {1.0f, 0.5f, 1.0f});
    EmitBox(verts, indices, {0.0f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f});
    m_stairs = MakeMesh(verts, indices);

    // Assign a sheet cell to every block that draws as a 3D icon.
    const auto count = static_cast<size_t>(BlockRegistry::Get().Count());
    m_cell.assign(count, -1);
    m_columns = kColumns;
    int next = 0;
    for (BlockId id = 1; id < static_cast<BlockId>(count); ++id) {
        if (Has3dIcon(id)) {
            m_cell[id] = next++;
        }
    }
}

BlockIcons::~BlockIcons() = default;

void BlockIcons::Build(int cellPx) {
    m_cellPx = cellPx;

    int cellCount = 0;
    for (int c : m_cell) {
        if (c >= 0) {
            ++cellCount;
        }
    }
    const int rows = (cellCount + m_columns - 1) / m_columns;
    const auto sheetW = static_cast<uint32_t>(m_columns * cellPx);
    const auto sheetH = static_cast<uint32_t>(std::max(rows, 1) * cellPx);
    m_sheet = std::make_unique<vox::Framebuffer>(sheetW, sheetH);

    m_sheet->Bind();
    vox::Renderer::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    vox::Renderer::Clear();
    vox::Renderer::SetCullFace(false); // both windings drawn; depth sorts

    // y-up ortho in sheet pixels (matches DrawImage's stored-bottom-left
    // sampling, so icons come out upright). Generous z range for the rotated
    // cube.
    const glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(sheetW), 0.0f,
                                      static_cast<float>(sheetH), -1000.0f, 1000.0f);

    m_shader->Bind();
    m_shader->SetInt("u_atlas", 0);
    m_atlas->Bind(0);

    const float cell = static_cast<float>(cellPx);
    for (BlockId id = 1; id < static_cast<BlockId>(m_cell.size()); ++id) {
        const int idx = m_cell[id];
        if (idx < 0) {
            continue;
        }
        const int col = idx % m_columns;
        const int row = idx / m_columns;
        // Cell center in framebuffer (bottom-left) coordinates.
        const float cx = static_cast<float>(col) * cell + cell * 0.5f;
        const float cy = static_cast<float>(sheetH) - (static_cast<float>(row) * cell + cell * 0.5f);

        glm::mat4 m = glm::translate(proj, {cx, cy, 0.0f});
        m = glm::scale(m, glm::vec3(cell));        // vanilla slot scale (its 16)
        m = glm::scale(m, glm::vec3(0.625f));      // block.json gui display scale
        m = glm::rotate(m, glm::radians(30.0f), {1.0f, 0.0f, 0.0f});
        m = glm::rotate(m, glm::radians(225.0f), {0.0f, 1.0f, 0.0f});
        m = glm::translate(m, glm::vec3(-0.5f));   // center the 0..1 model

        const BlockDef& def = BlockRegistry::Get().Def(id);
        for (int f = 0; f < 6; ++f) {
            m_shader->SetFloat(std::format("u_faceLayers[{}]", f),
                               static_cast<float>(def.faceTiles[static_cast<size_t>(f)]));
        }
        m_shader->SetMat4("u_mvp", m);
        const vox::VertexArray& mesh = def.stairs ? *m_stairs : def.slab ? *m_slab : *m_cube;
        vox::Renderer::DrawIndexed(mesh);
    }

    vox::Renderer::SetCullFace(true);
    vox::Framebuffer::Unbind();
}

void BlockIcons::EnsureBuilt(int cellPx, uint32_t windowWidth, uint32_t windowHeight) {
    if (cellPx != m_cellPx || !m_sheet) {
        Build(cellPx);
    }
    vox::Renderer::SetViewport(windowWidth, windowHeight);
}

bool BlockIcons::IconRect(ItemId id, glm::vec2& srcPos, glm::vec2& srcSize) const {
    if (id >= m_cell.size() || m_cell[id] < 0 || !m_sheet) {
        return false;
    }
    const int idx = m_cell[id];
    const int col = idx % m_columns;
    const int row = idx / m_columns;
    const float cell = static_cast<float>(m_cellPx);
    srcPos = {static_cast<float>(col) * cell, static_cast<float>(row) * cell};
    srcSize = {cell, cell};
    return true;
}

const std::shared_ptr<vox::Texture2D>& BlockIcons::Sheet() const {
    return m_sheet->Color();
}

} // namespace vc
