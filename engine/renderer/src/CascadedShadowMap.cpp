#include "puluo/renderer/CascadedShadowMap.h"
#include "puluo/renderer/Camera.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Puluo {

CascadedShadowMap::~CascadedShadowMap() {
    Destroy();
}

void CascadedShadowMap::Create(const CSMConfig& config) {
    Destroy();
    m_Config = config;

    // Create FBO
    glGenFramebuffers(1, &m_FBO);

    // Create depth texture array
    glGenTextures(1, &m_DepthTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_DepthTextureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 config.resolution, config.resolution, config.cascadeCount,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Enable hardware shadow comparison for PCF
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    PULUO_CORE_INFO("CSM created: {0}x{0}, {1} cascades", config.resolution, config.cascadeCount);
}

void CascadedShadowMap::Destroy() {
    if (m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
        m_FBO = 0;
    }
    if (m_DepthTextureArray) {
        glDeleteTextures(1, &m_DepthTextureArray);
        m_DepthTextureArray = 0;
    }
}

std::vector<Vec4> CascadedShadowMap::GetFrustumCornersWorldSpace(
    const Mat4& proj, const Mat4& view) const
{
    Mat4 inv = glm::inverse(proj * view);
    std::vector<Vec4> corners;
    corners.reserve(8);
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < 2; z++) {
                Vec4 pt = inv * Vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f,
                    1.0f);
                corners.push_back(pt / pt.w);
            }
        }
    }
    return corners;
}

Mat4 CascadedShadowMap::ComputeLightSpaceMatrix(
    float nearClip, float farClip, float fov, float aspect,
    const Mat4& viewMatrix, const Vec3& lightDir) const
{
    Mat4 proj = glm::perspective(glm::radians(fov), aspect, nearClip, farClip);
    auto corners = GetFrustumCornersWorldSpace(proj, viewMatrix);

    // Compute frustum center
    Vec3 center(0.0f);
    for (auto& c : corners) {
        center += Vec3(c);
    }
    center /= static_cast<float>(corners.size());

    // Compute bounding sphere radius (max distance from center to any corner).
    // This is constant for a given split range and FOV, so the ortho bounds
    // stay stable as the camera moves — preventing shadow popping.
    float radius = 0.0f;
    for (auto& c : corners) {
        float d = glm::length(Vec3(c) - center);
        radius = std::max(radius, d);
    }
    // Round up to texel boundary to avoid sub-texel jitter
    float texelSize = (radius * 2.0f) / static_cast<float>(m_Config.resolution);
    radius = std::ceil(radius / texelSize) * texelSize;

    // Light view matrix
    Vec3 up(0.0f, 1.0f, 0.0f);
    float upDot = std::abs(glm::dot(glm::normalize(lightDir), up));
    if (upDot > 0.99f) {
        up = Vec3(0.0f, 0.0f, 1.0f);
    }
    Mat4 lightView = glm::lookAt(center + lightDir, center, up);

    // Snap the center position to texel grid in light space to prevent swimming
    Vec4 centerLS = lightView * Vec4(center, 1.0f);
    centerLS.x = std::floor(centerLS.x / texelSize) * texelSize;
    centerLS.y = std::floor(centerLS.y / texelSize) * texelSize;
    // Reconstruct snapped center in world space and rebuild light view
    Mat4 invLightView = glm::inverse(lightView);
    Vec3 snappedCenter = Vec3(invLightView * centerLS);
    lightView = glm::lookAt(snappedCenter + lightDir, snappedCenter, up);

    // Use bounding sphere for stable ortho bounds (constant size)
    float minX = -radius;
    float maxX =  radius;
    float minY = -radius;
    float maxY =  radius;

    // Compute Z range from corners in snapped light space
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (auto& c : corners) {
        Vec4 lc = lightView * c;
        minZ = std::min(minZ, lc.z);
        maxZ = std::max(maxZ, lc.z);
    }

    // Extend Z range generously to capture shadow casters behind the frustum
    float zRange = maxZ - minZ;
    minZ -= zRange * 1.0f;

    Mat4 lightProjection = glm::orthoZO(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}

void CascadedShadowMap::Update(const CameraController& camera, const Vec3& lightDirection) {
    float nearClip = camera.GetNearClip();
    float farClip = std::min(camera.GetFarClip(), m_Config.shadowDistance);
    float fov = camera.GetFov();
    float aspect = camera.GetAspectRatio();
    Mat4 viewMatrix = camera.GetViewMatrix();
    uint32_t count = m_Config.cascadeCount;

    // Practical split scheme (logarithmic/uniform blend)
    std::vector<float> splits(count + 1);
    splits[0] = nearClip;
    for (uint32_t i = 1; i <= count; i++) {
        float p = static_cast<float>(i) / static_cast<float>(count);
        float logSplit = nearClip * std::pow(farClip / nearClip, p);
        float uniformSplit = nearClip + (farClip - nearClip) * p;
        splits[i] = m_Config.splitLambda * logSplit + (1.0f - m_Config.splitLambda) * uniformSplit;
    }

    // Store cascade split distances (for shader cascade selection)
    for (uint32_t i = 0; i < count; i++) {
        m_CascadeSplits[i] = splits[i + 1];
    }

    // Compute light-space matrix for each cascade
    Vec3 dir = glm::normalize(lightDirection);
    for (uint32_t i = 0; i < count; i++) {
        m_LightSpaceMatrices[i] = ComputeLightSpaceMatrix(
            splits[i], splits[i + 1], fov, aspect, viewMatrix, dir);
    }
}

void CascadedShadowMap::BindCascade(uint32_t index) const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              m_DepthTextureArray, 0, index);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glViewport(0, 0, m_Config.resolution, m_Config.resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void CascadedShadowMap::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadedShadowMap::BindShadowMapTexture(uint32_t slot) const {
    glBindTextureUnit(slot, m_DepthTextureArray);
}

} // namespace Puluo
