#include "puluo/renderer/TAA.h"
#include "puluo/renderer/Shader.h"
#include <glad/gl.h>

namespace Puluo {

void TAA::Create(uint32_t width, uint32_t height) {
    if (m_FBO != 0) Destroy();

    m_Width = width;
    m_Height = height;

    // Create FBO
    glCreateFramebuffers(1, &m_FBO);

    // Create output and history textures (GL_RGBA16F for HDR precision)
    glCreateTextures(GL_TEXTURE_2D, 1, &m_OutputTexture);
    glTextureStorage2D(m_OutputTexture, 1, GL_RGBA16F, width, height);
    glTextureParameteri(m_OutputTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_OutputTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_OutputTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_OutputTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_HistoryTexture);
    glTextureStorage2D(m_HistoryTexture, 1, GL_RGBA16F, width, height);
    glTextureParameteri(m_HistoryTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_HistoryTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_HistoryTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_HistoryTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Clear history to black (first frame has no history)
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(m_HistoryTexture, 0, GL_RGBA, GL_FLOAT, clearColor);

    // Attach output texture to FBO
    glNamedFramebufferTexture(m_FBO, GL_COLOR_ATTACHMENT0, m_OutputTexture, 0);

    // Check FBO completeness
    if (glCheckNamedFramebufferStatus(m_FBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Destroy();
        return;
    }

    // Load TAA shader
    m_TAAShader = Shader::CreateFromFile("assets/shaders/taa_resolve.vert",
                                          "assets/shaders/taa_resolve.frag");
}

void TAA::Destroy() {
    if (m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
        m_FBO = 0;
    }
    if (m_OutputTexture) {
        glDeleteTextures(1, &m_OutputTexture);
        m_OutputTexture = 0;
    }
    if (m_HistoryTexture) {
        glDeleteTextures(1, &m_HistoryTexture);
        m_HistoryTexture = 0;
    }
    m_TAAShader.reset();
    m_Width = 0;
    m_Height = 0;
}

void TAA::Resize(uint32_t width, uint32_t height) {
    if (width == m_Width && height == m_Height) return;
    Destroy();
    Create(width, height);
}

void TAA::Resolve(uint32_t currentColor, uint32_t depthTexture,
                  const Mat4& currentVP, const Mat4& prevVP,
                  Vec2 jitter, Vec2 prevJitter,
                  uint32_t emptyVAO) {
    if (!IsCreated() || !m_TAAShader) return;

    // Bind FBO (output texture as target)
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);

    // Bind shader
    m_TAAShader->Bind();

    // Set uniforms
    m_TAAShader->SetInt("uCurrentColor", 0);
    m_TAAShader->SetInt("uHistoryColor", 1);
    m_TAAShader->SetInt("uDepthTexture", 2);

    Mat4 currentVPInverse = glm::inverse(currentVP);
    m_TAAShader->SetMat4("uCurrentVPInverse", currentVPInverse);
    m_TAAShader->SetMat4("uPrevVP", prevVP);
    m_TAAShader->SetVec2("uScreenSize", Vec2(static_cast<float>(m_Width), static_cast<float>(m_Height)));
    // Jitter in pixel units for unjitter correction
    m_TAAShader->SetVec2("uJitter", jitter * Vec2(static_cast<float>(m_Width), static_cast<float>(m_Height)) * 0.5f);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, currentColor);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_HistoryTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, depthTexture);

    // Draw fullscreen triangle
    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Swap output and history for next frame
    std::swap(m_OutputTexture, m_HistoryTexture);
    // Update FBO attachment to new output
    glNamedFramebufferTexture(m_FBO, GL_COLOR_ATTACHMENT0, m_OutputTexture, 0);
}

} // namespace Puluo
