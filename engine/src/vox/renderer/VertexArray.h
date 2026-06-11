#pragma once

#include <memory>
#include <vector>

#include "vox/renderer/Buffer.h"

namespace vox {

class VertexArray {
public:
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    void Bind() const;

    // The buffer's layout must be set before adding.
    void AddVertexBuffer(std::shared_ptr<VertexBuffer> buffer);
    void SetIndexBuffer(std::shared_ptr<IndexBuffer> buffer);

    const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return m_indexBuffer; }

private:
    uint32_t m_handle = 0;
    uint32_t m_attribIndex = 0;
    uint32_t m_bindingIndex = 0;
    std::vector<std::shared_ptr<VertexBuffer>> m_vertexBuffers;
    std::shared_ptr<IndexBuffer> m_indexBuffer;
};

} // namespace vox
