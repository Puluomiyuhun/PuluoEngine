#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/VertexArray.h"
#include "puluo/renderer/Buffer.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace Puluo {

struct TerrainParams {
    float worldSize = 500.0f;       // terrain side length in world units
    int heightmapRes = 257;         // heightmap resolution (2^n+1)
    int patchCount = 64;            // patches per side
    float heightScale = 80.0f;      // max height displacement
    float uvScale = 50.0f;          // material texture tiling
};

// Single PBR layer (albedo + normal + roughness)
struct TerrainLayerMaterial {
    uint32_t albedoTex = 0;
    uint32_t normalTex = 0;
    uint32_t roughnessTex = 0;
    std::string albedoPath;
    std::string normalPath;
    std::string roughnessPath;
    float normalStrength = 1.0f;
};

struct TerrainMaterial {
    TerrainLayerMaterial lower;   // height < threshold
    TerrainLayerMaterial upper;   // height >= threshold
    TerrainLayerMaterial slope;   // steep areas (highest priority)

    // Blend parameters
    float heightThreshold = 0.5f;   // split point for upper/lower [0,1]
    float slopeThreshold  = 0.6f;   // normal.y < (1-slopeThreshold) → slope material
    float blendSharpness  = 8.0f;   // smoothstep transition sharpness
};

// Terrain raycast hit result
struct TerrainHit {
    bool hit = false;
    Vec3 position{0.0f};
};

class Terrain {
public:
    Terrain() = default;
    ~Terrain();

    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    void Create(const TerrainParams& params);
    void Destroy();
    void GenerateFromNoise(float frequency = 3.0f, int octaves = 6);
    bool LoadFromImage(const std::string& imagePath);

    float GetHeightAt(float worldX, float worldZ) const;

    // Splat map (RGB: R=Lower, G=Upper, B=Slope, each 0-255, R+G+B=255)
    void GenerateSplatFromRules(float heightThreshold, float slopeThreshold, float blendSharpness);
    void PaintSplat(float worldX, float worldZ, int layer, float radius, float strength, bool erase = false);
    void UploadSplatMap();
    bool SaveSplatMap(const std::string& path) const;
    bool LoadSplatMap(const std::string& path);
    bool HasSplatMap() const { return m_SplatMapTex != 0; }
    uint32_t GetSplatMapTexture() const { return m_SplatMapTex; }

    // Raycast from screen coordinates against terrain heightmap
    TerrainHit Raycast(const Vec3& rayOrigin, const Vec3& rayDir) const;

    uint32_t GetHeightmapTexture() const { return m_HeightmapTex; }
    const std::shared_ptr<VertexArray>& GetPatchVAO() const { return m_PatchVAO; }
    int GetPatchVertexCount() const { return m_PatchVertexCount; }
    const TerrainParams& GetParams() const { return m_Params; }
    bool IsCreated() const { return m_Created; }

private:
    TerrainParams m_Params;
    bool m_Created = false;
    std::vector<float> m_HeightData; // CPU-side heightmap [0,1]
    uint32_t m_HeightmapTex = 0;     // GPU R32F texture

    // Splat map (CPU + GPU)
    std::vector<unsigned char> m_SplatData; // RGB interleaved, size = res*res*3
    uint32_t m_SplatMapTex = 0;

    std::shared_ptr<VertexArray> m_PatchVAO;
    int m_PatchVertexCount = 0;

    void GeneratePatchMesh();
    void UploadHeightmap();

    // Helper: compute slope at a heightmap pixel
    float ComputeSlopeAt(int px, int py) const;
};

} // namespace Puluo
