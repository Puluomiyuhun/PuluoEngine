#pragma once

#include "puluo/renderer/Mesh.h"
#include "puluo/renderer/Texture2D.h"
#include "puluo/renderer/Material.h"
#include <string>
#include <vector>
#include <memory>

namespace Puluo {

struct ModelMesh {
    Mesh mesh;
    int materialIndex = -1;
    AABB localAABB;
};

struct PBRMaterialData {
    Vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;

    std::shared_ptr<Texture2D> albedoMap;
    std::shared_ptr<Texture2D> normalMap;
    std::shared_ptr<Texture2D> metallicMap;
    std::shared_ptr<Texture2D> roughnessMap;
    std::shared_ptr<Texture2D> aoMap;

    // --- 植物材质 ---
    std::shared_ptr<Texture2D> maskMap;       // Alpha mask (slot 10)
    float alphaCutoff = 0.5f;                 // Discard 阈值
    bool useAlphaMask = false;                // 启用 mask 剔除

    bool useSSS = false;                      // 启用次表面散射
    Vec3 sssColor{0.5f, 0.8f, 0.2f};         // SSS 颜色扰动
    float sssStrength = 0.5f;                 // SSS 强度 [0, 2]
};

class AssetLoader; // Forward declaration

class Model {
public:
    Model() = default;

    bool Load(const std::string& filepath);

    const std::vector<ModelMesh>& GetMeshes() const { return m_Meshes; }
    const std::vector<PBRMaterialData>& GetMaterials() const { return m_Materials; }
    std::vector<PBRMaterialData>& GetMutableMaterials() { return m_Materials; }
    const std::string& GetDirectory() const { return m_Directory; }
    const AABB& GetBoundingBox() const { return m_BoundingBox; }

private:
    friend class AssetLoader;
    std::vector<ModelMesh> m_Meshes;
    std::vector<PBRMaterialData> m_Materials;
    std::string m_Directory;
    AABB m_BoundingBox;

    std::shared_ptr<Texture2D> LoadTextureFromScene(const std::string& ref, const void* scene);
};

} // namespace Puluo
