#pragma once

#include "puluo/renderer/Buffer.h"
#include <memory>
#include <vector>

namespace Puluo {

class VertexArray {
public:
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer(std::shared_ptr<VertexBuffer> vbo);
    void AddInstanceBuffer(std::shared_ptr<VertexBuffer> vbo);
    void SetIndexBuffer(std::shared_ptr<IndexBuffer> ebo);

    const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const { return m_VertexBuffers; }
    const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_VertexBufferIndex = 0;
    std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
    std::shared_ptr<IndexBuffer> m_IndexBuffer;
};

} // namespace Puluo
