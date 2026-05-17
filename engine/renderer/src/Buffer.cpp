#include "puluo/renderer/Buffer.h"
#include "puluo/core/Base.h"

#include <glad/gl.h>

namespace Puluo {

// ---- ShaderDataType helpers ----

uint32_t ShaderDataTypeSize(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float:  return 4;
        case ShaderDataType::Float2: return 4 * 2;
        case ShaderDataType::Float3: return 4 * 3;
        case ShaderDataType::Float4: return 4 * 4;
        case ShaderDataType::Int:    return 4;
        case ShaderDataType::Int2:   return 4 * 2;
        case ShaderDataType::Int3:   return 4 * 3;
        case ShaderDataType::Int4:   return 4 * 4;
        case ShaderDataType::Mat3:   return 4 * 3 * 3;
        case ShaderDataType::Mat4:   return 4 * 4 * 4;
        case ShaderDataType::Bool:   return 1;
    }
    PULUO_CORE_ASSERT(false, "Unknown ShaderDataType!");
    return 0;
}

uint32_t ShaderDataTypeComponentCount(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float:  return 1;
        case ShaderDataType::Float2: return 2;
        case ShaderDataType::Float3: return 3;
        case ShaderDataType::Float4: return 4;
        case ShaderDataType::Int:    return 1;
        case ShaderDataType::Int2:   return 2;
        case ShaderDataType::Int3:   return 3;
        case ShaderDataType::Int4:   return 4;
        case ShaderDataType::Mat3:   return 3 * 3;
        case ShaderDataType::Mat4:   return 4 * 4;
        case ShaderDataType::Bool:   return 1;
    }
    PULUO_CORE_ASSERT(false, "Unknown ShaderDataType!");
    return 0;
}

static GLenum ShaderDataTypeToGL(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float:
        case ShaderDataType::Float2:
        case ShaderDataType::Float3:
        case ShaderDataType::Float4:
        case ShaderDataType::Mat3:
        case ShaderDataType::Mat4:   return GL_FLOAT;
        case ShaderDataType::Int:
        case ShaderDataType::Int2:
        case ShaderDataType::Int3:
        case ShaderDataType::Int4:   return GL_INT;
        case ShaderDataType::Bool:   return GL_BOOL;
    }
    return 0;
}

// ---- BufferElement ----

BufferElement::BufferElement(const std::string& name, ShaderDataType type, bool normalized)
    : name(name), type(type), size(ShaderDataTypeSize(type)), offset(0), normalized(normalized) {}

// ---- BufferLayout ----

BufferLayout::BufferLayout(std::initializer_list<BufferElement> elements)
    : m_Elements(elements)
{
    CalculateOffsetsAndStride();
}

void BufferLayout::CalculateOffsetsAndStride() {
    uint32_t offset = 0;
    m_Stride = 0;
    for (auto& element : m_Elements) {
        element.offset = offset;
        offset += element.size;
    }
    m_Stride = offset;
}

// ---- VertexBuffer ----

VertexBuffer::VertexBuffer(const void* data, uint32_t size) {
    glCreateBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

VertexBuffer::VertexBuffer(uint32_t size) {
    glCreateBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
}

VertexBuffer::~VertexBuffer() {
    if (m_RendererID)
        glDeleteBuffers(1, &m_RendererID);
}

VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept
    : m_RendererID(other.m_RendererID), m_Layout(std::move(other.m_Layout))
{
    other.m_RendererID = 0;
}

VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept {
    if (this != &other) {
        if (m_RendererID) glDeleteBuffers(1, &m_RendererID);
        m_RendererID = other.m_RendererID;
        m_Layout = std::move(other.m_Layout);
        other.m_RendererID = 0;
    }
    return *this;
}

void VertexBuffer::Bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
}

void VertexBuffer::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::SetData(const void* data, uint32_t size) {
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

// ---- IndexBuffer ----

IndexBuffer::IndexBuffer(const uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    glCreateBuffers(1, &m_RendererID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}

IndexBuffer::~IndexBuffer() {
    if (m_RendererID)
        glDeleteBuffers(1, &m_RendererID);
}

IndexBuffer::IndexBuffer(IndexBuffer&& other) noexcept
    : m_RendererID(other.m_RendererID), m_Count(other.m_Count)
{
    other.m_RendererID = 0;
    other.m_Count = 0;
}

IndexBuffer& IndexBuffer::operator=(IndexBuffer&& other) noexcept {
    if (this != &other) {
        if (m_RendererID) glDeleteBuffers(1, &m_RendererID);
        m_RendererID = other.m_RendererID;
        m_Count = other.m_Count;
        other.m_RendererID = 0;
        other.m_Count = 0;
    }
    return *this;
}

void IndexBuffer::Bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
}

void IndexBuffer::Unbind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// ---- UniformBuffer ----

UniformBuffer::UniformBuffer(uint32_t size, uint32_t binding) {
    glCreateBuffers(1, &m_RendererID);
    glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
}

UniformBuffer::~UniformBuffer() {
    if (m_RendererID)
        glDeleteBuffers(1, &m_RendererID);
}

void UniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
    glNamedBufferSubData(m_RendererID, offset, size, data);
}

} // namespace Puluo
