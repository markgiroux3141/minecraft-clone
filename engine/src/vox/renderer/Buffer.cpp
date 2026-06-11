#include "vox/renderer/Buffer.h"

#include <glad/gl.h>

#include "vox/core/Assert.h"

namespace vox {

uint32_t ShaderDataTypeSize(ShaderDataType type) {
    switch (type) {
    case ShaderDataType::Float:
    case ShaderDataType::Int:
    case ShaderDataType::UInt:
        return 4;
    case ShaderDataType::Float2:
    case ShaderDataType::Int2:
        return 8;
    case ShaderDataType::Float3:
    case ShaderDataType::Int3:
        return 12;
    case ShaderDataType::Float4:
    case ShaderDataType::Int4:
        return 16;
    }
    VOX_ASSERT(false, "Unknown ShaderDataType");
    return 0;
}

uint32_t ShaderDataTypeComponentCount(ShaderDataType type) {
    switch (type) {
    case ShaderDataType::Float:
    case ShaderDataType::Int:
    case ShaderDataType::UInt:
        return 1;
    case ShaderDataType::Float2:
    case ShaderDataType::Int2:
        return 2;
    case ShaderDataType::Float3:
    case ShaderDataType::Int3:
        return 3;
    case ShaderDataType::Float4:
    case ShaderDataType::Int4:
        return 4;
    }
    VOX_ASSERT(false, "Unknown ShaderDataType");
    return 0;
}

VertexBuffer::VertexBuffer(const void* data, uint32_t size) {
    glCreateBuffers(1, &m_handle);
    glNamedBufferStorage(m_handle, size, data, 0);
}

VertexBuffer::VertexBuffer(uint32_t size) {
    glCreateBuffers(1, &m_handle);
    glNamedBufferStorage(m_handle, size, nullptr, GL_DYNAMIC_STORAGE_BIT);
}

VertexBuffer::~VertexBuffer() {
    glDeleteBuffers(1, &m_handle);
}

void VertexBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
    glNamedBufferSubData(m_handle, offset, size, data);
}

IndexBuffer::IndexBuffer(const uint32_t* indices, uint32_t count) : m_count(count) {
    glCreateBuffers(1, &m_handle);
    glNamedBufferStorage(m_handle, static_cast<GLsizeiptr>(count) * sizeof(uint32_t), indices, 0);
}

IndexBuffer::~IndexBuffer() {
    glDeleteBuffers(1, &m_handle);
}

} // namespace vox
