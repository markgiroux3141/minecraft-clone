#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "vox/renderer/Buffer.h"

namespace vox {

// Pools many small meshes that share one vertex layout into a single GPU
// vertex buffer and draws any subset of them with one
// glMultiDrawElementsIndirect call.
//
// Index data is implicit: every mesh must be a sequence of fixed-size
// primitive groups (e.g. quads = 4 vertices) that all use the same index
// pattern, so one shared pattern index buffer (the pattern repeated with
// a per-group vertex offset) serves every mesh in the pool — meshes store
// no index data at all.
//
// Allocation is a first-fit free list over the vertex buffer, coalescing
// on Free and growing the buffer (copy to a larger one) when full. Each
// draw item carries a vec4 the shader can read as
//   layout(std430, binding = 0) readonly buffer ... { vec4 u_perDraw[]; };
// indexed by gl_DrawID (the typical use is a per-mesh world offset).
//
// Main-thread-only, like every GL-owning class.
class MeshPool {
public:
    using MeshHandle = uint32_t;
    static constexpr MeshHandle kInvalidMesh = 0xFFFFFFFFu;

    struct DrawItem {
        MeshHandle mesh;
        glm::vec4 perDraw; // u_perDraw[gl_DrawID] in the shader
    };

    // indexPattern indexes vertices [0, verticesPerGroup) of one group;
    // initialVertexCapacity is in vertices, not bytes.
    MeshPool(BufferLayout layout, std::span<const uint32_t> indexPattern,
             uint32_t verticesPerGroup, uint32_t initialVertexCapacity);
    ~MeshPool();

    MeshPool(const MeshPool&) = delete;
    MeshPool& operator=(const MeshPool&) = delete;

    // vertexCount must be a non-zero multiple of verticesPerGroup.
    MeshHandle Allocate(const void* vertexData, uint32_t vertexCount);
    void Free(MeshHandle handle);

    // One multi-draw of the items (triangles). Binds its own VAO, the
    // indirect buffer, and the per-draw SSBO at binding 0; the caller
    // binds the shader and its uniforms/textures.
    void Draw(std::span<const DrawItem> items);

    // Indices drawn for this mesh: groups * pattern size.
    uint32_t IndexCount(MeshHandle handle) const;

    uint32_t UsedVertices() const { return m_usedVertices; }
    uint32_t CapacityVertices() const { return m_capacityVertices; }
    size_t MeshCount() const { return m_meshes.size() - m_freeHandles.size(); }

private:
    struct Mesh {
        uint32_t offset = 0; // in vertices
        uint32_t vertexCount = 0;
        bool active = false;
    };
    struct FreeBlock {
        uint32_t offset; // in vertices
        uint32_t size;
    };

    void GrowVertexBuffer(uint32_t minExtraVertices);
    void EnsureIndexCapacity(uint32_t groups);
    void InsertFreeBlock(FreeBlock block); // keeps m_free sorted, coalesces

    BufferLayout m_layout;
    std::vector<uint32_t> m_indexPattern;
    uint32_t m_verticesPerGroup;

    uint32_t m_vao = 0;
    uint32_t m_vertexBuffer = 0;
    uint32_t m_indexBuffer = 0;
    uint32_t m_indirectBuffer = 0;
    uint32_t m_perDrawBuffer = 0;

    uint32_t m_capacityVertices = 0;
    uint32_t m_usedVertices = 0;
    uint32_t m_indexCapacityGroups = 0;

    std::vector<Mesh> m_meshes;
    std::vector<MeshHandle> m_freeHandles;
    std::vector<FreeBlock> m_free; // sorted by offset, no adjacent blocks

    // Per-Draw scratch, kept to avoid reallocating every frame.
    std::vector<uint32_t> m_commandScratch;
    std::vector<glm::vec4> m_perDrawScratch;
};

} // namespace vox
