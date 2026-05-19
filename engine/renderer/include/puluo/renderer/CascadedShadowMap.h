#pragma once

#include "puluo/core/Math.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Puluo {

class CameraController;

struct CSMConfig {
    uint32_t resolution = 2048;
    uint32_t cascadeCount = 4;
    float splitLambda = 0.75f;      // practical split scheme blend factor
    float shadowDistance = 500.0f;   // max shadow distance
    float depthBiasConstant = 2.5f;
    float depthBiasSlope = 2.5f;
    float normalBias = 0.8f;        // world-space normal offset in shader
    float shadowIntensity = 0.85f;   // 0=no shadow, 1=fully dark shadow
};

class CascadedShadowMap {
public:
    static constexpr uint32_t MAX_CASCADES = 4;

    CascadedShadowMap() = default;
    ~CascadedShadowMap();

    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;

    void Create(const CSMConfig& config);
    void Destroy();

    // Recompute cascade splits and light-space VP matrices.
    void Update(const CameraController& camera, const Vec3& lightDirection);

    // Bind FBO for rendering cascade `index`. Sets viewport, clears depth.
    void BindCascade(uint32_t index) const;
    void Unbind() const;

    // Bind texture array for shadow sampling.
    void BindShadowMapTexture(uint32_t slot) const;

    const Mat4& GetLightSpaceMatrix(uint32_t cascade) const { return m_LightSpaceMatrices[cascade]; }
    const std::array<Mat4, MAX_CASCADES>& GetLightSpaceMatrices() const { return m_LightSpaceMatrices; }
    const std::array<float, MAX_CASCADES>& GetCascadeSplits() const { return m_CascadeSplits; }

    uint32_t GetTextureArrayID() const { return m_DepthTextureArray; }
    uint32_t GetResolution() const { return m_Config.resolution; }
    uint32_t GetCascadeCount() const { return m_Config.cascadeCount; }
    const CSMConfig& GetConfig() const { return m_Config; }
    CSMConfig& GetConfig() { return m_Config; }

    bool IsCreated() const { return m_FBO != 0; }

private:
    CSMConfig m_Config;

    uint32_t m_FBO = 0;
    uint32_t m_DepthTextureArray = 0;

    std::array<Mat4, MAX_CASCADES> m_LightSpaceMatrices{};
    std::array<float, MAX_CASCADES> m_CascadeSplits{};

    std::vector<Vec4> GetFrustumCornersWorldSpace(const Mat4& proj, const Mat4& view) const;
    Mat4 ComputeLightSpaceMatrix(float nearClip, float farClip, float fov, float aspect,
                                  const Mat4& viewMatrix, const Vec3& lightDir) const;
};

} // namespace Puluo
