#include "puluo/renderer/Mesh.h"
#include "puluo/renderer/RenderCommand.h"

#include <cmath>

namespace Puluo {

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    m_VAO = std::make_shared<VertexArray>();

    m_VBO = std::make_shared<VertexBuffer>(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(Vertex)));
    m_VBO->SetLayout({
        {"aPosition", ShaderDataType::Float3},
        {"aNormal",   ShaderDataType::Float3},
        {"aTexCoord", ShaderDataType::Float2},
        {"aTangent",  ShaderDataType::Float3}
    });
    m_VAO->AddVertexBuffer(m_VBO);

    m_EBO = std::make_shared<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));
    m_VAO->SetIndexBuffer(m_EBO);
}

void Mesh::Draw() const {
    RenderCommand::DrawIndexed(m_VAO);
}

Mesh Mesh::CreateCube() {
    // Each face has its own tangent
    std::vector<Vertex> vertices = {
        // Front face (z = +0.5), tangent = +X
        {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {0, 0}, {1, 0, 0}},
        {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1, 0}, {1, 0, 0}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1, 1}, {1, 0, 0}},
        {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {0, 1}, {1, 0, 0}},
        // Back face (z = -0.5), tangent = -X
        {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {0, 0}, {-1, 0, 0}},
        {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1, 0}, {-1, 0, 0}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1, 1}, {-1, 0, 0}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {0, 1}, {-1, 0, 0}},
        // Top face (y = +0.5), tangent = +X
        {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {0, 0}, {1, 0, 0}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1, 0}, {1, 0, 0}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1, 1}, {1, 0, 0}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {0, 1}, {1, 0, 0}},
        // Bottom face (y = -0.5), tangent = +X
        {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {0, 0}, {1, 0, 0}},
        {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1, 0}, {1, 0, 0}},
        {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1, 1}, {1, 0, 0}},
        {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {0, 1}, {1, 0, 0}},
        // Right face (x = +0.5), tangent = +Z (inverted)
        {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {0, 0}, {0, 0,-1}},
        {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1, 0}, {0, 0,-1}},
        {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1, 1}, {0, 0,-1}},
        {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {0, 1}, {0, 0,-1}},
        // Left face (x = -0.5), tangent = +Z
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}, {0, 0, 1}},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}, {0, 0, 1}},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}, {0, 0, 1}},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}, {0, 0, 1}},
    };

    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  3,  0,
        4,  5,  6,  6,  7,  4,
        8,  9,  10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    return Mesh(vertices, indices);
}

Mesh Mesh::CreatePlane(float size) {
    float half = size * 0.5f;
    std::vector<Vertex> vertices = {
        {{-half, 0, -half}, {0, 1, 0}, {0, 0}, {1, 0, 0}},
        {{ half, 0, -half}, {0, 1, 0}, {size, 0}, {1, 0, 0}},
        {{ half, 0,  half}, {0, 1, 0}, {size, size}, {1, 0, 0}},
        {{-half, 0,  half}, {0, 1, 0}, {0, size}, {1, 0, 0}},
    };
    std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };
    return Mesh(vertices, indices);
}

Mesh Mesh::CreateSphere(uint32_t segments, uint32_t rings) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t y = 0; y <= rings; y++) {
        for (uint32_t x = 0; x <= segments; x++) {
            float xSeg = static_cast<float>(x) / static_cast<float>(segments);
            float ySeg = static_cast<float>(y) / static_cast<float>(rings);
            float xPos = std::cos(xSeg * 2.0f * 3.14159265f) * std::sin(ySeg * 3.14159265f);
            float yPos = std::cos(ySeg * 3.14159265f);
            float zPos = std::sin(xSeg * 2.0f * 3.14159265f) * std::sin(ySeg * 3.14159265f);

            Vec3 normal = glm::normalize(Vec3(xPos, yPos, zPos));
            // Tangent: derivative of position w.r.t. xSeg (longitude)
            Vec3 tangent = glm::normalize(Vec3(
                -std::sin(xSeg * 2.0f * 3.14159265f),
                0.0f,
                std::cos(xSeg * 2.0f * 3.14159265f)
            ));

            vertices.push_back({
                Vec3(xPos, yPos, zPos),
                normal,
                Vec2(xSeg, ySeg),
                tangent
            });
        }
    }

    for (uint32_t y = 0; y < rings; y++) {
        for (uint32_t x = 0; x < segments; x++) {
            uint32_t i0 = y * (segments + 1) + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (y + 1) * (segments + 1) + x;
            uint32_t i3 = i2 + 1;

            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }

    return Mesh(vertices, indices);
}

} // namespace Puluo
