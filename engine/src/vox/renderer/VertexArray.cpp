#include "vox/renderer/VertexArray.h"

#include <glad/gl.h>

#include "vox/core/Assert.h"

namespace vox {

namespace {

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

} // namespace

VertexArray::VertexArray() {
    glCreateVertexArrays(1, &m_handle);
}

VertexArray::~VertexArray() {
    glDeleteVertexArrays(1, &m_handle);
}

void VertexArray::Bind() const {
    glBindVertexArray(m_handle);
}

void VertexArray::AddVertexBuffer(std::shared_ptr<VertexBuffer> buffer) {
    const auto& layout = buffer->Layout();
    VOX_ASSERT(!layout.Elements().empty(), "Vertex buffer has no layout");

    glVertexArrayVertexBuffer(m_handle, m_bindingIndex, buffer->Handle(), 0,
                              static_cast<GLsizei>(layout.Stride()));

    for (const auto& element : layout) {
        glEnableVertexArrayAttrib(m_handle, m_attribIndex);
        if (IsIntegerType(element.type)) {
            glVertexArrayAttribIFormat(m_handle, m_attribIndex,
                                       static_cast<GLint>(ShaderDataTypeComponentCount(element.type)),
                                       BaseGlType(element.type), element.offset);
        } else {
            glVertexArrayAttribFormat(m_handle, m_attribIndex,
                                      static_cast<GLint>(ShaderDataTypeComponentCount(element.type)),
                                      BaseGlType(element.type),
                                      element.normalized ? GL_TRUE : GL_FALSE, element.offset);
        }
        glVertexArrayAttribBinding(m_handle, m_attribIndex, m_bindingIndex);
        ++m_attribIndex;
    }

    ++m_bindingIndex;
    m_vertexBuffers.push_back(std::move(buffer));
}

void VertexArray::SetIndexBuffer(std::shared_ptr<IndexBuffer> buffer) {
    glVertexArrayElementBuffer(m_handle, buffer->Handle());
    m_indexBuffer = std::move(buffer);
}

} // namespace vox
