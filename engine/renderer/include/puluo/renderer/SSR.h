#pragma once

#include "puluo/renderer/Shader.h"
#include "puluo/core/Math.h"

#include <memory>
#include <cstdint>

namespace Puluo {

struct SSRConfig {
    bool enabled = false;
    int maxSteps = 64;
    float maxDistance = 50.0f;
    float thickness = 0.5f;
    float fadeEdge = 0.1f;
    int binarySearchSteps = 5;
};

class SSR {
public:
    ~SSR();

    void Create(uint32_t width, uint32_t height);
    void Destroy();
    void Resize(uint32_t width, uint32_t height);

    void Generate(uint32_t depthTexture, uint32_t sceneColorTexture,
                  const Mat4& projection, const Mat4& view,
                  const SSRConfig& config, const Vec2& screenSize,
                  uint32_t emptyVAO);

    uint32_t GetReflectionTexture() const { return m_ColorBuffer; }
    void BindTexture(uint32_t slot) const;
    bool IsCreated() const { return m_FBO != 0; }

private:
    void CreateFBO(uint32_t w, uint32_t h);
    void DestroyFBO();

    uint32_t m_FBO = 0;
    uint32_t m_ColorBuffer = 0;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    std::shared_ptr<Shader> m_Shader;
};

} // namespace Puluo
