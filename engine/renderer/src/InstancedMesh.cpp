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

    // Compute per-instance world-space AABBs and centers
    m_InstanceAABBs.resize(m_InstanceCount);
    m_Centers.resize(m_InstanceCount);
    m_CulledMatrices.resize(m_InstanceCount);

    if (m_Model && m_InstanceCount > 0) {
        const auto& localAABB = m_Model->GetBoundingBox();

        for (uint32_t i = 0; i < m_InstanceCount; i++) {
            m_InstanceAABBs[i] = TransformAABB(localAABB, m_Matrices[i]);

            // Center for distance culling
            Vec3 localCenter = (localAABB.min + localAABB.max) * 0.5f;
            Vec4 wc = m_Matrices[i] * Vec4(localCenter, 1.0f);
            m_Centers[i] = Vec3(wc);
        }
    }

    // Recompute world AABB (reuse per-instance AABBs already computed above)
    m_WorldAABB = AABB{};
    for (uint32_t i = 0; i < m_InstanceCount; i++) {
        m_WorldAABB.Merge(m_InstanceAABBs[i]);
    }

    m_VisibleCount = m_InstanceCount;
}

void InstancedMesh::CullAndUpload(const Frustum& frustum, const Vec3& camPos, float maxDistance) {
    m_VisibleCount = 0;
    if (!m_Model || m_InstanceCount == 0) return;

    for (uint32_t i = 0; i < m_InstanceCount; i++) {
        // Distance culling (use closest point on AABB to camera)
        if (maxDistance > 0.0f) {
            Vec3 closest = glm::clamp(camPos, m_InstanceAABBs[i].min, m_InstanceAABBs[i].max);
            float dist = glm::distance(closest, camPos);
            if (dist > maxDistance) continue;
        }

        // Frustum culling (AABB test — tighter than sphere, no false negatives)
        if (!frustum.TestAABB(m_InstanceAABBs[i])) continue;

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
