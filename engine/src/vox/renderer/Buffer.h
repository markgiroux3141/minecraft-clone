#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace vox {

enum class ShaderDataType : uint8_t {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UInt,
};

uint32_t ShaderDataTypeSize(ShaderDataType type);
uint32_t ShaderDataTypeComponentCount(ShaderDataType type);

struct BufferElement {
    std::string name;
    ShaderDataType type;
    uint32_t offset = 0; // filled in by BufferLayout
    bool normalized = false;

    BufferElement(ShaderDataType type, std::string name, bool normalized = false)
        : name(std::move(name)), type(type), normalized(normalized) {}
};

class BufferLayout {
public:
    BufferLayout() = default;
    BufferLayout(std::initializer_list<BufferElement> elements) : m_elements(elements) {
        for (auto& element : m_elements) {
            element.offset = m_stride;
            m_stride += ShaderDataTypeSize(element.type);
        }
    }

    uint32_t Stride() const { return m_stride; }
    const std::vector<BufferElement>& Elements() const { return m_elements; }

    auto begin() const { return m_elements.begin(); }
    auto end() const { return m_elements.end(); }

private:
    std::vector<BufferElement> m_elements;
    uint32_t m_stride = 0;
};

class VertexBuffer {
public:
    // Immutable contents, uploaded once.
    VertexBuffer(const void* data, uint32_t size);
    // Allocated but empty, for streaming via SetData.
    explicit VertexBuffer(uint32_t size);
    ~VertexBuffer();

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    void SetData(const void* data, uint32_t size, uint32_t offset = 0);

    const BufferLayout& Layout() const { return m_layout; }
    void SetLayout(BufferLayout layout) { m_layout = std::move(layout); }

    uint32_t Handle() const { return m_handle; }

private:
    uint32_t m_handle = 0;
    BufferLayout m_layout;
};

class IndexBuffer {
public:
    IndexBuffer(const uint32_t* indices, uint32_t count);
    ~IndexBuffer();

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

    uint32_t Count() const { return m_count; }
    uint32_t Handle() const { return m_handle; }

private:
    uint32_t m_handle = 0;
    uint32_t m_count = 0;
};

} // namespace vox
