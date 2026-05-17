#include "puluo/renderer/VertexArray.h"
#include "puluo/core/Base.h"

#include <glad/gl.h>

namespace Puluo {

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

VertexArray::VertexArray() {
    glCreateVertexArrays(1, &m_RendererID);
}

VertexArray::~VertexArray() {
    if (m_RendererID)
        glDeleteVertexArrays(1, &m_RendererID);
}

void VertexArray::Bind() const {
    glBindVertexArray(m_RendererID);
}

void VertexArray::Unbind() const {
    glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(std::shared_ptr<VertexBuffer> vbo) {
    PULUO_CORE_ASSERT(vbo->GetLayout().GetElements().size(), "VertexBuffer has no layout!");

    glBindVertexArray(m_RendererID);
    vbo->Bind();

    const auto& layout = vbo->GetLayout();
    for (const auto& element : layout) {
        uint32_t componentCount = ShaderDataTypeComponentCount(element.type);

        switch (element.type) {
            case ShaderDataType::Float:
            case ShaderDataType::Float2:
            case ShaderDataType::Float3:
            case ShaderDataType::Float4: {
                glEnableVertexAttribArray(m_VertexBufferIndex);
                glVertexAttribPointer(
                    m_VertexBufferIndex,
                    componentCount,
                    ShaderDataTypeToGL(element.type),
                    element.normalized ? GL_TRUE : GL_FALSE,
                    layout.GetStride(),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(element.offset))
                );
                m_VertexBufferIndex++;
                break;
            }
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:
            case ShaderDataType::Bool: {
                glEnableVertexAttribArray(m_VertexBufferIndex);
                glVertexAttribIPointer(
                    m_VertexBufferIndex,
                    componentCount,
                    ShaderDataTypeToGL(element.type),
                    layout.GetStride(),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(element.offset))
                );
                m_VertexBufferIndex++;
                break;
            }
            case ShaderDataType::Mat3:
            case ShaderDataType::Mat4: {
                uint32_t count = (element.type == ShaderDataType::Mat3) ? 3 : 4;
                for (uint32_t i = 0; i < count; i++) {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribPointer(
                        m_VertexBufferIndex,
                        count,
                        GL_FLOAT,
                        element.normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        reinterpret_cast<const void*>(static_cast<uintptr_t>(element.offset + sizeof(float) * count * i))
                    );
                    glVertexAttribDivisor(m_VertexBufferIndex, 1);
                    m_VertexBufferIndex++;
                }
                break;
            }
        }
    }

    m_VertexBuffers.push_back(std::move(vbo));
}

void VertexArray::AddInstanceBuffer(std::shared_ptr<VertexBuffer> vbo) {
    PULUO_CORE_ASSERT(vbo->GetLayout().GetElements().size(), "VertexBuffer has no layout!");

    glBindVertexArray(m_RendererID);
    vbo->Bind();

    const auto& layout = vbo->GetLayout();
    for (const auto& element : layout) {
        uint32_t componentCount = ShaderDataTypeComponentCount(element.type);

        switch (element.type) {
            case ShaderDataType::Float:
            case ShaderDataType::Float2:
            case ShaderDataType::Float3:
            case ShaderDataType::Float4: {
                glEnableVertexAttribArray(m_VertexBufferIndex);
                glVertexAttribPointer(
                    m_VertexBufferIndex,
                    componentCount,
                    GL_FLOAT,
                    element.normalized ? GL_TRUE : GL_FALSE,
                    layout.GetStride(),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(element.offset))
                );
                glVertexAttribDivisor(m_VertexBufferIndex, 1);
                m_VertexBufferIndex++;
                break;
            }
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:
            case ShaderDataType::Bool: {
                glEnableVertexAttribArray(m_VertexBufferIndex);
                glVertexAttribIPointer(
                    m_VertexBufferIndex,
                    componentCount,
                    ShaderDataTypeToGL(element.type),
                    layout.GetStride(),
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(element.offset))
                );
                glVertexAttribDivisor(m_VertexBufferIndex, 1);
                m_VertexBufferIndex++;
                break;
            }
            case ShaderDataType::Mat3:
            case ShaderDataType::Mat4: {
                uint32_t count = (element.type == ShaderDataType::Mat3) ? 3 : 4;
                for (uint32_t i = 0; i < count; i++) {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribPointer(
                        m_VertexBufferIndex,
                        count,
                        GL_FLOAT,
                        element.normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        reinterpret_cast<const void*>(static_cast<uintptr_t>(element.offset + sizeof(float) * count * i))
                    );
                    glVertexAttribDivisor(m_VertexBufferIndex, 1);
                    m_VertexBufferIndex++;
                }
                break;
            }
            default:
                break;
        }
    }

    m_VertexBuffers.push_back(std::move(vbo));
}

void VertexArray::SetIndexBuffer(std::shared_ptr<IndexBuffer> ebo) {
    glBindVertexArray(m_RendererID);
    ebo->Bind();
    m_IndexBuffer = std::move(ebo);
}

} // namespace Puluo
