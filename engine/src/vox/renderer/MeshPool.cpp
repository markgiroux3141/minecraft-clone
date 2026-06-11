#include "vox/renderer/MeshPool.h"

#include <algorithm>

#include <glad/gl.h>

#include "vox/core/Assert.h"
#include "vox/core/Log.h"

namespace vox {

namespace {

// Mirrors the attribute setup in VertexArray.cpp.
bool IsIntegerType(ShaderDataType type) {
    switch (type) {
    case ShaderDataType::Int:
    case ShaderDataType::Int2:
    case ShaderDataType::Int3:
    case ShaderDataType::Int4:
    case ShaderDataType::UInt:
        return true;
    default:
        return false;
    }
}

GLenum BaseGlType(ShaderDataType type) {
    switch (type) {
    case ShaderDataType::UInt:
        return GL_UNSIGNED_INT;
    case ShaderDataType::Int:
    case ShaderDataType::Int2:
    case ShaderDataType::Int3:
    case ShaderDataType::Int4:
        return GL_INT;
    default:
        return GL_FLOAT;
    }
}

struct DrawElementsIndirectCommand {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    uint32_t baseInstance;
};

} // namespace

MeshPool::MeshPool(BufferLayout layout, std::span<const uint32_t> indexPattern,
                   uint32_t verticesPerGroup, uint32_t initialVertexCapacity)
    : m_layout(std::move(layout)), m_indexPattern(indexPattern.begin(), indexPattern.end()),
      m_verticesPerGroup(verticesPerGroup), m_capacityVertices(initialVertexCapacity) {
    VOX_ASSERT(!m_layout.Elements().empty(), "MeshPool needs a vertex layout");
    VOX_ASSERT(verticesPerGroup > 0 && !indexPattern.empty(), "MeshPool needs an index pattern");

    glCreateBuffers(1, &m_vertexBuffer);
    glNamedBufferStorage(m_vertexBuffer,
                         static_cast<GLsizeiptr>(m_capacityVertices) * m_layout.Stride(),
                         nullptr, GL_DYNAMIC_STORAGE_BIT);
    glCreateBuffers(1, &m_indirectBuffer);
    glCreateBuffers(1, &m_perDrawBuffer);

    glCreateVertexArrays(1, &m_vao);
    glVertexArrayVertexBuffer(m_vao, 0, m_vertexBuffer, 0,
                              static_cast<GLsizei>(m_layout.Stride()));
    uint32_t attrib = 0;
    for (const auto& element : m_layout) {
        glEnableVertexArrayAttrib(m_vao, attrib);
        if (IsIntegerType(element.type)) {
            glVertexArrayAttribIFormat(m_vao, attrib,
                                       static_cast<GLint>(ShaderDataTypeComponentCount(element.type)),
                                       BaseGlType(element.type), element.offset);
        } else {
            glVertexArrayAttribFormat(m_vao, attrib,
                                      static_cast<GLint>(ShaderDataTypeComponentCount(element.type)),
                                      BaseGlType(element.type),
                                      element.normalized ? GL_TRUE : GL_FALSE, element.offset);
        }
        glVertexArrayAttribBinding(m_vao, attrib, 0);
        ++attrib;
    }

    m_free.push_back({0, m_capacityVertices});
    EnsureIndexCapacity(1024);
}

MeshPool::~MeshPool() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vertexBuffer);
    glDeleteBuffers(1, &m_indexBuffer);
    glDeleteBuffers(1, &m_indirectBuffer);
    glDeleteBuffers(1, &m_perDrawBuffer);
}

void MeshPool::EnsureIndexCapacity(uint32_t groups) {
    if (groups <= m_indexCapacityGroups) {
        return;
    }
    m_indexCapacityGroups = std::max(groups, m_indexCapacityGroups * 2);

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(m_indexCapacityGroups) * m_indexPattern.size());
    for (uint32_t g = 0; g < m_indexCapacityGroups; ++g) {
        for (const uint32_t i : m_indexPattern) {
            indices.push_back(g * m_verticesPerGroup + i);
        }
    }
    glDeleteBuffers(1, &m_indexBuffer); // silently ignores 0
    glCreateBuffers(1, &m_indexBuffer);
    glNamedBufferStorage(m_indexBuffer,
                         static_cast<GLsizeiptr>(indices.size()) * sizeof(uint32_t),
                         indices.data(), 0);
    glVertexArrayElementBuffer(m_vao, m_indexBuffer);
}

void MeshPool::GrowVertexBuffer(uint32_t minExtraVertices) {
    const uint32_t oldCapacity = m_capacityVertices;
    m_capacityVertices = std::max(oldCapacity + minExtraVertices, oldCapacity * 2);
    VOX_INFO("MeshPool: growing vertex buffer {} -> {} vertices ({} MB)", oldCapacity,
             m_capacityVertices,
             static_cast<uint64_t>(m_capacityVertices) * m_layout.Stride() / (1024 * 1024));

    uint32_t newBuffer = 0;
    glCreateBuffers(1, &newBuffer);
    glNamedBufferStorage(newBuffer,
                         static_cast<GLsizeiptr>(m_capacityVertices) * m_layout.Stride(),
                         nullptr, GL_DYNAMIC_STORAGE_BIT);
    // Live allocations are scattered across the whole old buffer; copy it all.
    glCopyNamedBufferSubData(m_vertexBuffer, newBuffer, 0, 0,
                             static_cast<GLsizeiptr>(oldCapacity) * m_layout.Stride());
    glDeleteBuffers(1, &m_vertexBuffer);
    m_vertexBuffer = newBuffer;
    glVertexArrayVertexBuffer(m_vao, 0, m_vertexBuffer, 0,
                              static_cast<GLsizei>(m_layout.Stride()));

    InsertFreeBlock({oldCapacity, m_capacityVertices - oldCapacity});
}

void MeshPool::InsertFreeBlock(FreeBlock block) {
    const auto next = std::lower_bound(
        m_free.begin(), m_free.end(), block.offset,
        [](const FreeBlock& b, uint32_t offset) { return b.offset < offset; });
    const auto it = m_free.insert(next, block);
    // Coalesce with the following block, then the preceding one.
    if (it + 1 != m_free.end() && it->offset + it->size == (it + 1)->offset) {
        it->size += (it + 1)->size;
        m_free.erase(it + 1);
    }
    const auto idx = static_cast<size_t>(it - m_free.begin());
    if (idx > 0 && m_free[idx - 1].offset + m_free[idx - 1].size == m_free[idx].offset) {
        m_free[idx - 1].size += m_free[idx].size;
        m_free.erase(m_free.begin() + static_cast<ptrdiff_t>(idx));
    }
}

MeshPool::MeshHandle MeshPool::Allocate(const void* vertexData, uint32_t vertexCount) {
    VOX_ASSERT(vertexCount > 0 && vertexCount % m_verticesPerGroup == 0,
               "Mesh size must be a non-zero multiple of the group size");

    auto fit = std::find_if(m_free.begin(), m_free.end(),
                            [&](const FreeBlock& b) { return b.size >= vertexCount; });
    if (fit == m_free.end()) {
        GrowVertexBuffer(vertexCount);
        fit = std::find_if(m_free.begin(), m_free.end(),
                           [&](const FreeBlock& b) { return b.size >= vertexCount; });
    }
    const uint32_t offset = fit->offset;
    fit->offset += vertexCount;
    fit->size -= vertexCount;
    if (fit->size == 0) {
        m_free.erase(fit);
    }

    glNamedBufferSubData(m_vertexBuffer,
                         static_cast<GLintptr>(offset) * m_layout.Stride(),
                         static_cast<GLsizeiptr>(vertexCount) * m_layout.Stride(), vertexData);
    EnsureIndexCapacity(vertexCount / m_verticesPerGroup);
    m_usedVertices += vertexCount;

    MeshHandle handle;
    if (!m_freeHandles.empty()) {
        handle = m_freeHandles.back();
        m_freeHandles.pop_back();
    } else {
        handle = static_cast<MeshHandle>(m_meshes.size());
        m_meshes.emplace_back();
    }
    m_meshes[handle] = {offset, vertexCount, true};
    return handle;
}

void MeshPool::Free(MeshHandle handle) {
    VOX_ASSERT(handle < m_meshes.size() && m_meshes[handle].active, "Freeing a dead mesh handle");
    Mesh& mesh = m_meshes[handle];
    InsertFreeBlock({mesh.offset, mesh.vertexCount});
    m_usedVertices -= mesh.vertexCount;
    mesh.active = false;
    m_freeHandles.push_back(handle);
}

uint32_t MeshPool::IndexCount(MeshHandle handle) const {
    const Mesh& mesh = m_meshes[handle];
    return mesh.vertexCount / m_verticesPerGroup * static_cast<uint32_t>(m_indexPattern.size());
}

void MeshPool::Draw(std::span<const DrawItem> items) {
    if (items.empty()) {
        return;
    }

    static_assert(sizeof(DrawElementsIndirectCommand) == 5 * sizeof(uint32_t));
    m_commandScratch.clear();
    m_commandScratch.reserve(items.size() * 5);
    m_perDrawScratch.clear();
    m_perDrawScratch.reserve(items.size());
    for (const DrawItem& item : items) {
        const Mesh& mesh = m_meshes[item.mesh];
        VOX_ASSERT(mesh.active, "Drawing a dead mesh handle");
        m_commandScratch.insert(m_commandScratch.end(),
                                {IndexCount(item.mesh), 1u, 0u, mesh.offset, 0u});
        m_perDrawScratch.push_back(item.perDraw);
    }

    // Orphaned every frame; tens of KB, not worth persistent mapping yet.
    glNamedBufferData(m_indirectBuffer,
                      static_cast<GLsizeiptr>(m_commandScratch.size()) * sizeof(uint32_t),
                      m_commandScratch.data(), GL_STREAM_DRAW);
    glNamedBufferData(m_perDrawBuffer,
                      static_cast<GLsizeiptr>(m_perDrawScratch.size()) * sizeof(glm::vec4),
                      m_perDrawScratch.data(), GL_STREAM_DRAW);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_perDrawBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(items.size()), 0);
}

} // namespace vox
