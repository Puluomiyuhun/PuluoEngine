#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/VertexArray.h"
#include "puluo/renderer/Buffer.h"
#include <vector>
#include <memory>

namespace Puluo {

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
    Vec3 tangent;
};

class Mesh {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    void Draw() const;

    const std::shared_ptr<VertexArray>& GetVertexArray() const { return m_VAO; }

    static Mesh CreateCube();
    static Mesh CreatePlane(float size = 10.0f);
    static Mesh CreateSphere(uint32_t segments = 32, uint32_t rings = 16);

private:
    std::shared_ptr<VertexArray> m_VAO;
    std::shared_ptr<VertexBuffer> m_VBO;
    std::shared_ptr<IndexBuffer> m_EBO;
};

} // namespace Puluo
