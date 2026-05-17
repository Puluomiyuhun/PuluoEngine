#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Puluo {

// Shader data types for vertex attribute description
enum class ShaderDataType : uint8_t {
    Float, Float2, Float3, Float4,
    Int, Int2, Int3, Int4,
    Mat3, Mat4,
    Bool
};

uint32_t ShaderDataTypeSize(ShaderDataType type);
uint32_t ShaderDataTypeComponentCount(ShaderDataType type);

struct BufferElement {
    std::string name;
    ShaderDataType type;
    uint32_t size;
    uint32_t offset;
    bool normalized;

    BufferElement(const std::string& name, ShaderDataType type, bool normalized = false);
};

class BufferLayout {
public:
    BufferLayout() = default;
    BufferLayout(std::initializer_list<BufferElement> elements);

    const std::vector<BufferElement>& GetElements() const { return m_Elements; }
    uint32_t GetStride() const { return m_Stride; }

    auto begin() const { return m_Elements.begin(); }
    auto end() const { return m_Elements.end(); }

private:
    void CalculateOffsetsAndStride();
    std::vector<BufferElement> m_Elements;
    uint32_t m_Stride = 0;
};

// ---- VertexBuffer ----

class VertexBuffer {
public:
    VertexBuffer(const void* data, uint32_t size);
    VertexBuffer(uint32_t size);  // dynamic
    ~VertexBuffer();

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&& other) noexcept;
    VertexBuffer& operator=(VertexBuffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;
    void SetData(const void* data, uint32_t size);

    void SetLayout(const BufferLayout& layout) { m_Layout = layout; }
    const BufferLayout& GetLayout() const { return m_Layout; }

private:
    uint32_t m_RendererID = 0;
    BufferLayout m_Layout;
};

// ---- IndexBuffer ----

class IndexBuffer {
public:
    IndexBuffer(const uint32_t* indices, uint32_t count);
    ~IndexBuffer();

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;
    IndexBuffer(IndexBuffer&& other) noexcept;
    IndexBuffer& operator=(IndexBuffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;
    uint32_t GetCount() const { return m_Count; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_Count = 0;
};

// ---- UniformBuffer ----

class UniformBuffer {
public:
    UniformBuffer(uint32_t size, uint32_t binding);
    ~UniformBuffer();

    void SetData(const void* data, uint32_t size, uint32_t offset = 0);

private:
    uint32_t m_RendererID = 0;
};

} // namespace Puluo
