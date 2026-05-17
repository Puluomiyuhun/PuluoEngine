#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/Shader.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace Puluo {

struct SSAOConfig {
    uint32_t kernelSize = 32;
    float radius = 0.5f;
    float bias = 0.025f;
    float power = 2.0f;
    bool enabled = true;
};

class SSAO {
public:
    SSAO() = default;
    ~SSAO();

    SSAO(const SSAO&) = delete;
    SSAO& operator=(const SSAO&) = delete;

    void Create(uint32_t width, uint32_t height, const SSAOConfig& config = {});
    void Destroy();
    void Resize(uint32_t width, uint32_t height);

    // Generate SSAO from a depth texture. Uses emptyVAO for fullscreen triangle.
    void Generate(uint32_t depthTexture, const Mat4& projection, uint32_t emptyVAO);

    uint32_t GetBlurredTexture() const { return m_BlurColorBuffer; }
    void BindTexture(uint32_t slot) const;

    SSAOConfig& GetConfig() { return m_Config; }
    const SSAOConfig& GetConfig() const { return m_Config; }
    bool IsCreated() const { return m_SSAOFBO != 0; }

    // Update kernel size at runtime (regenerates kernel if changed)
    void SetKernelSize(uint32_t size);

private:
    SSAOConfig m_Config;

    // Half-resolution FBOs
    uint32_t m_SSAOFBO = 0;
    uint32_t m_SSAOColorBuffer = 0;   // GL_R8
    uint32_t m_BlurFBO = 0;
    uint32_t m_BlurColorBuffer = 0;   // GL_R8

    uint32_t m_NoiseTexture = 0;      // 4x4 GL_RGBA16F
    std::vector<Vec3> m_Kernel;

    std::shared_ptr<Shader> m_SSAOShader;
    std::shared_ptr<Shader> m_BlurShader;

    uint32_t m_Width = 0, m_Height = 0; // half-res dimensions

    void GenerateKernel();
    void GenerateNoiseTexture();
    void CreateFBOs(uint32_t halfW, uint32_t halfH);
    void DestroyFBOs();
};

} // namespace Puluo
