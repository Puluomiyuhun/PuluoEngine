#include "puluo/renderer/SSAO.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <algorithm>

namespace Puluo {

SSAO::~SSAO() {
    Destroy();
}

void SSAO::Create(uint32_t width, uint32_t height, const SSAOConfig& config) {
    m_Config = config;

    GenerateKernel();
    GenerateNoiseTexture();

    m_Width = std::max(width / 2, 1u);
    m_Height = std::max(height / 2, 1u);
    CreateFBOs(m_Width, m_Height);

    // Load shaders — reuse fxaa.vert for fullscreen triangle
    m_SSAOShader = Shader::CreateFromFile("assets/shaders/fxaa.vert",
                                           "assets/shaders/ssao/ssao.frag");
    m_BlurShader = Shader::CreateFromFile("assets/shaders/fxaa.vert",
                                           "assets/shaders/ssao/ssao_blur.frag");

    PULUO_INFO("SSAO created: {}x{} (half-res), {} samples", m_Width, m_Height, m_Config.kernelSize);
}

void SSAO::Destroy() {
    DestroyFBOs();
    if (m_NoiseTexture) {
        glDeleteTextures(1, &m_NoiseTexture);
        m_NoiseTexture = 0;
    }
    m_Kernel.clear();
    m_SSAOShader.reset();
    m_BlurShader.reset();
}

void SSAO::Resize(uint32_t width, uint32_t height) {
    uint32_t halfW = std::max(width / 2, 1u);
    uint32_t halfH = std::max(height / 2, 1u);
    if (halfW == m_Width && halfH == m_Height) return;
    m_Width = halfW;
    m_Height = halfH;
    DestroyFBOs();
    CreateFBOs(m_Width, m_Height);
}

void SSAO::CreateFBOs(uint32_t halfW, uint32_t halfH) {
    // SSAO FBO
    glCreateFramebuffers(1, &m_SSAOFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOFBO);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_SSAOColorBuffer);
    glBindTexture(GL_TEXTURE_2D, m_SSAOColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, halfW, halfH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SSAOColorBuffer, 0);

    // Blur FBO
    glCreateFramebuffers(1, &m_BlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_BlurColorBuffer);
    glBindTexture(GL_TEXTURE_2D, m_BlurColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, halfW, halfH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BlurColorBuffer, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::DestroyFBOs() {
    if (m_SSAOFBO) {
        glDeleteFramebuffers(1, &m_SSAOFBO);
        glDeleteTextures(1, &m_SSAOColorBuffer);
        m_SSAOFBO = 0;
        m_SSAOColorBuffer = 0;
    }
    if (m_BlurFBO) {
        glDeleteFramebuffers(1, &m_BlurFBO);
        glDeleteTextures(1, &m_BlurColorBuffer);
        m_BlurFBO = 0;
        m_BlurColorBuffer = 0;
    }
}

void SSAO::GenerateKernel() {
    std::default_random_engine rng(42); // Fixed seed for determinism
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    m_Kernel.resize(m_Config.kernelSize);
    for (uint32_t i = 0; i < m_Config.kernelSize; i++) {
        // Random direction in tangent-space hemisphere
        Vec3 sample(
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f,
            dist(rng)  // Z in [0,1] — hemisphere
        );
        sample = glm::normalize(sample);
        sample *= dist(rng); // Random length

        // Accelerate: bias samples closer to origin
        float scale = static_cast<float>(i) / static_cast<float>(m_Config.kernelSize);
        scale = glm::mix(0.1f, 1.0f, scale * scale);
        sample *= scale;

        m_Kernel[i] = sample;
    }
}

void SSAO::GenerateNoiseTexture() {
    std::default_random_engine rng(123);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // 4x4 random rotation vectors (rotate around Z in tangent space)
    std::vector<float> noiseData(4 * 4 * 4); // 4x4 texels, RGBA
    for (int i = 0; i < 16; i++) {
        noiseData[i * 4 + 0] = dist(rng) * 2.0f - 1.0f; // X
        noiseData[i * 4 + 1] = dist(rng) * 2.0f - 1.0f; // Y
        noiseData[i * 4 + 2] = 0.0f;                      // Z = 0 (rotate in XY plane)
        noiseData[i * 4 + 3] = 0.0f;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &m_NoiseTexture);
    glBindTexture(GL_TEXTURE_2D, m_NoiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, noiseData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SSAO::SetKernelSize(uint32_t size) {
    size = std::clamp(size, 8u, 64u);
    if (size != m_Config.kernelSize) {
        m_Config.kernelSize = size;
        GenerateKernel();
    }
}

void SSAO::Generate(uint32_t depthTexture, const Mat4& projection, uint32_t emptyVAO) {
    if (!m_SSAOFBO || !m_SSAOShader || !m_BlurShader) return;

    // Save current state
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    Mat4 invProjection = glm::inverse(projection);

    // ---- Pass 1: SSAO generation ----
    glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOFBO);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_SSAOShader->Bind();
    m_SSAOShader->SetInt("uDepthTexture", 0);
    m_SSAOShader->SetInt("uNoiseTexture", 1);
    m_SSAOShader->SetMat4("uProjection", projection);
    m_SSAOShader->SetMat4("uInvProjection", invProjection);
    m_SSAOShader->SetVec2("uNoiseScale", Vec2(
        static_cast<float>(m_Width) / 4.0f,
        static_cast<float>(m_Height) / 4.0f));
    m_SSAOShader->SetInt("uKernelSize", static_cast<int>(m_Config.kernelSize));
    m_SSAOShader->SetFloat("uRadius", m_Config.radius);
    m_SSAOShader->SetFloat("uBias", m_Config.bias);
    m_SSAOShader->SetFloat("uPower", m_Config.power);

    // Upload kernel samples
    for (uint32_t i = 0; i < m_Config.kernelSize; i++) {
        m_SSAOShader->SetVec3("uSamples[" + std::to_string(i) + "]", m_Kernel[i]);
    }

    glBindTextureUnit(0, depthTexture);
    glBindTextureUnit(1, m_NoiseTexture);

    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // ---- Pass 2: Blur ----
    glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    m_BlurShader->Bind();
    m_BlurShader->SetInt("uSSAOInput", 0);
    glBindTextureUnit(0, m_SSAOColorBuffer);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void SSAO::BindTexture(uint32_t slot) const {
    glBindTextureUnit(slot, m_BlurColorBuffer);
}

} // namespace Puluo
