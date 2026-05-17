#include "puluo/renderer/SSR.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

namespace Puluo {

SSR::~SSR() {
    Destroy();
}

void SSR::Create(uint32_t width, uint32_t height) {
    m_Width = width;
    m_Height = height;
    CreateFBO(m_Width, m_Height);

    m_Shader = Shader::CreateFromFile("assets/shaders/fxaa.vert",
                                       "assets/shaders/ssr.frag");

    PULUO_INFO("SSR created: {}x{}", m_Width, m_Height);
}

void SSR::Destroy() {
    DestroyFBO();
    m_Shader.reset();
}

void SSR::Resize(uint32_t width, uint32_t height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyFBO();
    CreateFBO(m_Width, m_Height);
}

void SSR::CreateFBO(uint32_t w, uint32_t h) {
    glCreateFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorBuffer);
    glBindTexture(GL_TEXTURE_2D, m_ColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorBuffer, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSR::DestroyFBO() {
    if (m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
        glDeleteTextures(1, &m_ColorBuffer);
        m_FBO = 0;
        m_ColorBuffer = 0;
    }
}

void SSR::Generate(uint32_t depthTexture, uint32_t sceneColorTexture,
                   const Mat4& projection, const Mat4& view,
                   const SSRConfig& config, const Vec2& screenSize,
                   uint32_t emptyVAO) {
    if (!m_FBO || !m_Shader) return;

    // Save current state
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    Mat4 invProjection = glm::inverse(projection);
    Mat4 invView = glm::inverse(view);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_Shader->Bind();
    m_Shader->SetInt("uDepthTexture", 0);
    m_Shader->SetInt("uSceneColor", 1);
    m_Shader->SetMat4("uProjection", projection);
    m_Shader->SetMat4("uInvProjection", invProjection);
    m_Shader->SetMat4("uView", view);
    m_Shader->SetMat4("uInvView", invView);
    m_Shader->SetVec2("uScreenSize", screenSize);
    m_Shader->SetInt("uMaxSteps", config.maxSteps);
    m_Shader->SetFloat("uMaxDistance", config.maxDistance);
    m_Shader->SetFloat("uThickness", config.thickness);
    m_Shader->SetFloat("uFadeEdge", config.fadeEdge);
    m_Shader->SetInt("uBinarySearchSteps", config.binarySearchSteps);

    glBindTextureUnit(0, depthTexture);
    glBindTextureUnit(1, sceneColorTexture);

    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void SSR::BindTexture(uint32_t slot) const {
    glBindTextureUnit(slot, m_ColorBuffer);
}

} // namespace Puluo
