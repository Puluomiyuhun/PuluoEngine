#include "puluo/renderer/InstancedMesh.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/Renderer.h"
#include "puluo/resource/ModelCache.h"
#include "puluo/core/Log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Puluo {

Mat4 InstanceTransform::ToMatrix() const {
    Mat4 mat{1.0f};
    mat = glm::translate(mat, position);
    mat = mat * glm::eulerAngleYXZ(
        glm::radians(rotation.y),
        glm::radians(rotation.x),
        glm::radians(rotation.z)
    );
    mat = glm::scale(mat, scale);
    return mat;
}

bool InstancedMesh::Setup(const std::string& modelPath, uint32_t maxInstances) {
    m_ModelPath = modelPath;
    m_MaxInstances = maxInstances;

    // Load the model via cache
    m_Model = ModelCache::Load(modelPath);
    if (!m_Model) {
        PULUO_CORE_ERROR("InstancedMesh: Failed to load model '{}'", modelPath);
        return false;
    }

    // Create dynamic instance VBO (Mat4 = 64 bytes per instance)
    m_InstanceVBO = std::make_shared<VertexBuffer>(maxInstances * static_cast<uint32_t>(sizeof(Mat4)));
    m_InstanceVBO->SetLayout({
        {"aInstanceModel", ShaderDataType::Mat4}
    });

    // Attach instance buffer to each mesh's VAO
    for (auto& modelMesh : m_Model->GetMeshes()) {
        // GetVertexArray returns const ref, but we need to modify the VAO during setup
        // Use const_cast here since Setup is a one-time initialization
        auto vao = modelMesh.mesh.GetVertexArray();
        vao->AddInstanceBuffer(m_InstanceVBO);
    }

    PULUO_CORE_INFO("InstancedMesh: Loaded '{}' with {} meshes, max {} instances",
                     modelPath, m_Model->GetMeshes().size(), maxInstances);
    return true;
}

void InstancedMesh::SetInstances(const std::vector<InstanceTransform>& transforms) {
    m_InstanceCount = static_cast<uint32_t>(
        std::min(transforms.size(), static_cast<size_t>(m_MaxInstances))
    );

    // Convert transforms to matrices
    m_Matrices.resize(m_InstanceCount);
    for (uint32_t i = 0; i < m_InstanceCount; i++) {
        m_Matrices[i] = transforms[i].ToMatrix();
    }

    // Upload to GPU
    if (m_InstanceCount > 0) {
        m_InstanceVBO->SetData(m_Matrices.data(), m_InstanceCount * sizeof(Mat4));
    }

    // Compute per-instance bounding spheres
    m_Centers.resize(m_InstanceCount);
    m_Radii.resize(m_InstanceCount);
    m_CulledMatrices.resize(m_InstanceCount);

    if (m_Model && m_InstanceCount > 0) {
        const auto& localAABB = m_Model->GetBoundingBox();
        Vec3 localCenter = (localAABB.min + localAABB.max) * 0.5f;
        float localRadius = glm::length(localAABB.max - localCenter);

        for (uint32_t i = 0; i < m_InstanceCount; i++) {
            // Transform center to world space
            Vec4 wc = m_Matrices[i] * Vec4(localCenter, 1.0f);
            m_Centers[i] = Vec3(wc);

            // Extract max scale from matrix column lengths
            float sx = glm::length(Vec3(m_Matrices[i][0]));
            float sy = glm::length(Vec3(m_Matrices[i][1]));
            float sz = glm::length(Vec3(m_Matrices[i][2]));
            float maxScale = std::max({sx, sy, sz});
            m_Radii[i] = localRadius * maxScale;
        }
    }

    // Recompute world AABB
    m_WorldAABB = AABB{};
    if (m_Model && m_InstanceCount > 0) {
        const auto& localAABB = m_Model->GetBoundingBox();
        for (uint32_t i = 0; i < m_InstanceCount; i++) {
            AABB instanceAABB = TransformAABB(localAABB, m_Matrices[i]);
            m_WorldAABB.Merge(instanceAABB);
        }
    }

    m_VisibleCount = m_InstanceCount;
}

void InstancedMesh::CullAndUpload(const Frustum& frustum, const Vec3& camPos, float maxDistance) {
    m_VisibleCount = 0;
    if (!m_Model || m_InstanceCount == 0) return;

    for (uint32_t i = 0; i < m_InstanceCount; i++) {
        // Distance culling
        float dist = glm::distance(m_Centers[i], camPos) - m_Radii[i];
        if (maxDistance > 0.0f && dist > maxDistance) continue;

        // Frustum culling
        if (!frustum.TestSphere(m_Centers[i], m_Radii[i])) continue;

        m_CulledMatrices[m_VisibleCount++] = m_Matrices[i];
    }

    if (m_VisibleCount > 0) {
        m_InstanceVBO->SetData(m_CulledMatrices.data(), m_VisibleCount * sizeof(Mat4));
    }
}

void InstancedMesh::DrawAllMeshesCulled() const {
    if (!m_Model || m_VisibleCount == 0) return;

    for (const auto& modelMesh : m_Model->GetMeshes()) {
        const auto& vao = modelMesh.mesh.GetVertexArray();
        RenderCommand::DrawIndexedInstanced(vao, m_VisibleCount);
    }
}

void InstancedMesh::DrawWithMaterialsCulled(const std::shared_ptr<Shader>& shader) const {
    if (!m_Model || m_VisibleCount == 0) return;

    const auto& materials = m_Model->GetMaterials();
    for (const auto& modelMesh : m_Model->GetMeshes()) {
        if (modelMesh.materialIndex >= 0 &&
            modelMesh.materialIndex < static_cast<int>(materials.size())) {
            Renderer::BindPBRMaterial(shader, materials[modelMesh.materialIndex]);
        }
        const auto& vao = modelMesh.mesh.GetVertexArray();
        RenderCommand::DrawIndexedInstanced(vao, m_VisibleCount);
    }
}

void InstancedMesh::DrawAllMeshes() const {
    if (!m_Model || m_InstanceCount == 0) return;

    for (const auto& modelMesh : m_Model->GetMeshes()) {
        const auto& vao = modelMesh.mesh.GetVertexArray();
        RenderCommand::DrawIndexedInstanced(vao, m_InstanceCount);
    }
}

void InstancedMesh::DrawWithMaterials(const std::shared_ptr<Shader>& shader) const {
    if (!m_Model || m_InstanceCount == 0) return;

    const auto& materials = m_Model->GetMaterials();
    for (const auto& modelMesh : m_Model->GetMeshes()) {
        if (modelMesh.materialIndex >= 0 &&
            modelMesh.materialIndex < static_cast<int>(materials.size())) {
            Renderer::BindPBRMaterial(shader, materials[modelMesh.materialIndex]);
        }
        const auto& vao = modelMesh.mesh.GetVertexArray();
        RenderCommand::DrawIndexedInstanced(vao, m_InstanceCount);
    }
}

} // namespace Puluo
