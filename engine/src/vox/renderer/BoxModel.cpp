#include "vox/renderer/BoxModel.h"

#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assert.h"
#include "vox/renderer/Buffer.h"
#include "vox/renderer/Renderer.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"
#include "vox/renderer/VertexArray.h"

namespace vox {

namespace {

// Vertex = position (pixels, part-local), normal, skin UV (0..1). 8 floats.
constexpr int kFloatsPerVertex = 8;

// Appends one cuboid's 6 faces to the vertex/index buffers, replicating
// vanilla ModelBox: the 8 corners, the six TexturedQuad UV islands laid out
// around the box's texOffset, and the loader's vertical texture flip (our
// Texture2D uploads bottom-row-first, so image V maps to t = 1 - V/H).
void AppendBox(std::vector<float>& verts, std::vector<uint32_t>& indices, const BoxModel::Box& box,
               float texW, float texH) {
    float x1 = box.origin.x - box.inflate;
    float y1 = box.origin.y - box.inflate;
    float z1 = box.origin.z - box.inflate;
    float x2 = box.origin.x + box.size.x + box.inflate;
    float y2 = box.origin.y + box.size.y + box.inflate;
    float z2 = box.origin.z + box.size.z + box.inflate;
    // Vanilla mirror swaps the X extents: each face then sits on the opposite
    // side of the box, so it samples the mirrored-across-X texture island.
    if (box.mirror) {
        std::swap(x1, x2);
    }

    const glm::vec3 p[8] = {
        {x1, y1, z1}, {x2, y1, z1}, {x2, y2, z1}, {x1, y2, z1},
        {x1, y1, z2}, {x2, y1, z2}, {x2, y2, z2}, {x1, y2, z2},
    };

    const float tu = box.texOffset.x;
    const float tv = box.texOffset.y;
    const float dx = box.size.x;
    const float dy = box.size.y;
    const float dz = box.size.z;

    // Each quad: the 4 box corners (vanilla TexturedQuad vertex order) and its
    // UV rect in image pixels (u1,v1 = the corner mapped to vertex[1]; the
    // mapping is vertex0->(u2,v1), 1->(u1,v1), 2->(u1,v2), 3->(u2,v2)). Y-down
    // model space: quad 2 is the box top, quad 3 the bottom.
    struct Quad {
        int v[4];
        float u1, v1, u2, v2;
        glm::vec3 normal;
    };
    const Quad quads[6] = {
        {{5, 1, 2, 6}, tu + dz + dx, tv + dz, tu + dz + dx + dz, tv + dz + dy, {1, 0, 0}},   // +X
        {{0, 4, 7, 3}, tu, tv + dz, tu + dz, tv + dz + dy, {-1, 0, 0}},                      // -X
        {{5, 4, 0, 1}, tu + dz, tv, tu + dz + dx, tv + dz, {0, -1, 0}},                      // top
        {{2, 3, 7, 6}, tu + dz + dx, tv + dz, tu + dz + dx + dx, tv, {0, 1, 0}},             // bottom
        {{1, 0, 3, 2}, tu + dz, tv + dz, tu + dz + dx, tv + dz + dy, {0, 0, -1}},            // -Z
        {{4, 5, 6, 7}, tu + dz + dx + dz, tv + dz, tu + dz + dx + dz + dx, tv + dz + dy, {0, 0, 1}}, // +Z
    };

    for (const Quad& q : quads) {
        const glm::vec2 uv[4] = {
            {q.u2, q.v1}, {q.u1, q.v1}, {q.u1, q.v2}, {q.u2, q.v2},
        };
        glm::vec3 n = q.normal;
        // The X swap above reflects the geometry across X — flip the X normal
        // so the relit faces shade correctly (cull is off, so winding is fine).
        if (box.mirror) {
            n.x = -n.x;
        }
        const auto base = static_cast<uint32_t>(verts.size() / kFloatsPerVertex);
        for (int c = 0; c < 4; ++c) {
            const glm::vec3 pos = p[q.v[c]];
            const float u = uv[c].x / texW;
            const float v = 1.0f - uv[c].y / texH;
            verts.insert(verts.end(), {pos.x, pos.y, pos.z, n.x, n.y, n.z, u, v});
        }
        indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }
}

} // namespace

int BoxModel::AddPart(std::string name, glm::vec3 pivot, std::vector<Box> boxes, int parent) {
    VOX_ASSERT(parent < static_cast<int>(m_parts.size()),
               "BoxModel parent must be added before its child");
    Part part;
    part.name = std::move(name);
    part.pivot = pivot;
    part.parent = parent;
    part.boxes = std::move(boxes);
    m_parts.push_back(std::move(part));
    return static_cast<int>(m_parts.size()) - 1;
}

int BoxModel::FindPart(std::string_view name) const {
    for (size_t i = 0; i < m_parts.size(); ++i) {
        if (m_parts[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void BoxModel::SetRotation(int part, glm::vec3 radians) {
    if (part >= 0 && part < static_cast<int>(m_parts.size())) {
        m_parts[static_cast<size_t>(part)].rotation = radians;
    }
}

void BoxModel::SetVisible(int part, bool visible) {
    if (part >= 0 && part < static_cast<int>(m_parts.size())) {
        m_parts[static_cast<size_t>(part)].visible = visible;
    }
}

void BoxModel::SetSkin(std::shared_ptr<Texture2D> skin, float texWidth, float texHeight) {
    m_skin = std::move(skin);
    m_texWidth = texWidth;
    m_texHeight = texHeight;
}

void BoxModel::Build() {
    for (Part& part : m_parts) {
        std::vector<float> verts;
        std::vector<uint32_t> indices;
        for (const Box& box : part.boxes) {
            AppendBox(verts, indices, box, m_texWidth, m_texHeight);
        }
        part.indexCount = static_cast<uint32_t>(indices.size());
        if (part.indexCount == 0) {
            continue;
        }
        auto buffer = std::make_shared<VertexBuffer>(
            verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float)));
        buffer->SetLayout({{ShaderDataType::Float3, "a_position"},
                           {ShaderDataType::Float3, "a_normal"},
                           {ShaderDataType::Float2, "a_uv"}});
        part.vao = std::make_shared<VertexArray>();
        part.vao->AddVertexBuffer(std::move(buffer));
        part.vao->SetIndexBuffer(
            std::make_shared<IndexBuffer>(indices.data(), part.indexCount));
    }
}

std::vector<glm::mat4> BoxModel::AccumulateTransforms(const glm::mat4& modelToWorld) const {
    // A part's local matrix is T(pivot) * Rz * Ry * Rx (vanilla glRotate order);
    // children compose onto the parent's accumulated matrix. Parents precede
    // children, so a single forward pass suffices.
    std::vector<glm::mat4> full(m_parts.size());
    for (size_t i = 0; i < m_parts.size(); ++i) {
        const Part& part = m_parts[i];
        glm::mat4 local = glm::translate(glm::mat4(1.0f), part.pivot);
        local = glm::rotate(local, part.rotation.z, {0.0f, 0.0f, 1.0f});
        local = glm::rotate(local, part.rotation.y, {0.0f, 1.0f, 0.0f});
        local = glm::rotate(local, part.rotation.x, {1.0f, 0.0f, 0.0f});
        const glm::mat4 parent =
            part.parent < 0 ? modelToWorld : full[static_cast<size_t>(part.parent)];
        full[i] = parent * local;
    }
    return full;
}

glm::mat4 BoxModel::PartTransform(int part, const glm::mat4& modelToWorld) const {
    if (part < 0 || part >= static_cast<int>(m_parts.size())) {
        return modelToWorld;
    }
    return AccumulateTransforms(modelToWorld)[static_cast<size_t>(part)];
}

void BoxModel::Render(Shader& shader, const glm::mat4& modelToWorld, int skinUnit) const {
    if (m_skin) {
        m_skin->Bind(static_cast<uint32_t>(skinUnit));
    }
    const std::vector<glm::mat4> full = AccumulateTransforms(modelToWorld);
    for (size_t i = 0; i < m_parts.size(); ++i) {
        const Part& part = m_parts[i];
        if (part.indexCount == 0 || !part.visible) {
            continue;
        }
        shader.SetMat4("u_model", full[i]);
        Renderer::DrawIndexed(*part.vao, part.indexCount);
    }
}

} // namespace vox
