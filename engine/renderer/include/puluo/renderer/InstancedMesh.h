#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/Model.h"
#include "puluo/renderer/Buffer.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/Frustum.h"
#include <vector>
#include <memory>
#include <string>

namespace Puluo {

struct InstanceTransform {
    Vec3 position{0.0f};
    Vec3 rotation{0.0f};  // Euler degrees (pitch, yaw, roll)
    Vec3 scale{1.0f};

    Mat4 ToMatrix() const;
};

class InstancedMesh {
public:
    InstancedMesh() = default;

    // Load the shared model, set up instance buffers on each mesh VAO
    bool Setup(const std::string& modelPath, uint32_t maxInstances = 4096);

    // Set/replace all instance transforms (rebuilds matrices, uploads to GPU)
    void SetInstances(const std::vector<InstanceTransform>& transforms);

    // Frustum + distance cull, compact visible matrices and upload to VBO
    void CullAndUpload(const Frustum& frustum, const Vec3& camPos, float maxDistance);

    // Draw using culled visible count (call CullAndUpload first)
    void DrawAllMeshesCulled() const;
    void DrawWithMaterialsCulled(const std::shared_ptr<Shader>& shader) const;

    // Draw all meshes instanced (for shadow/depth passes - no materials)
    void DrawAllMeshes() const;

    // Draw with PBR materials (for main pass)
    void DrawWithMaterials(const std::shared_ptr<Shader>& shader) const;

    // Accessors
    const std::shared_ptr<Model>& GetModel() const { return m_Model; }
    const std::string& GetModelPath() const { return m_ModelPath; }
    uint32_t GetInstanceCount() const { return m_InstanceCount; }
    uint32_t GetVisibleCount() const { return m_VisibleCount; }
    uint32_t GetMaxInstances() const { return m_MaxInstances; }
    const AABB& GetWorldAABB() const { return m_WorldAABB; }

private:
    std::shared_ptr<Model> m_Model;
    std::string m_ModelPath;
    uint32_t m_InstanceCount = 0;
    uint32_t m_VisibleCount = 0;
    uint32_t m_MaxInstances = 0;

    // One instance VBO shared across all mesh VAOs
    std::shared_ptr<VertexBuffer> m_InstanceVBO;

    // CPU-side matrix data (all instances)
    std::vector<Mat4> m_Matrices;

    // Per-instance world-space AABB (for frustum culling)
    std::vector<AABB> m_InstanceAABBs;

    // Per-instance bounding sphere center (for distance culling)
    std::vector<Vec3> m_Centers;

    // Compacted visible matrices (reused each frame to avoid alloc)
    std::vector<Mat4> m_CulledMatrices;

    // World-space AABB enclosing all instances (for coarse frustum culling)
    AABB m_WorldAABB;
};

} // namespace Puluo
