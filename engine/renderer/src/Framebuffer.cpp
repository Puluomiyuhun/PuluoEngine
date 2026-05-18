#include "puluo/renderer/Framebuffer.h"
#include "puluo/core/Base.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>

namespace Puluo {

Framebuffer::Framebuffer(const FramebufferSpec& spec)
    : m_Spec(spec)
{
    Invalidate();
}

Framebuffer::~Framebuffer() {
    if (m_RendererID) {
        glDeleteFramebuffers(1, &m_RendererID);
        if (m_ColorAttachment)
            glDeleteTextures(1, &m_ColorAttachment);
        if (m_EntityIDAttachment)
            glDeleteTextures(1, &m_EntityIDAttachment);
        if (m_DepthAttachment) {
            if (m_DepthIsTexture)
                glDeleteTextures(1, &m_DepthAttachment);
            else
                glDeleteRenderbuffers(1, &m_DepthAttachment);
        }
    }
    // Clean up resolve resources
    if (m_ResolveFBO) {
        glDeleteFramebuffers(1, &m_ResolveFBO);
        if (m_ResolveColorAttachment)
            glDeleteTextures(1, &m_ResolveColorAttachment);
        if (m_ResolveEntityIDAttachment)
            glDeleteTextures(1, &m_ResolveEntityIDAttachment);
    }
}

void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
    glViewport(0, 0, m_Spec.width, m_Spec.height);
}

void Framebuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    m_Spec.width = width;
    m_Spec.height = height;
    Invalidate();
}

void Framebuffer::Invalidate() {
    // --- Clean up existing resources ---
    if (m_RendererID) {
        glDeleteFramebuffers(1, &m_RendererID);
        if (m_ColorAttachment)
            glDeleteTextures(1, &m_ColorAttachment);
        if (m_EntityIDAttachment)
            glDeleteTextures(1, &m_EntityIDAttachment);
        if (m_DepthAttachment) {
            if (m_DepthIsTexture)
                glDeleteTextures(1, &m_DepthAttachment);
            else
                glDeleteRenderbuffers(1, &m_DepthAttachment);
        }
        m_ColorAttachment = 0;
        m_EntityIDAttachment = 0;
        m_DepthAttachment = 0;
    }
    if (m_ResolveFBO) {
        glDeleteFramebuffers(1, &m_ResolveFBO);
        if (m_ResolveColorAttachment)
            glDeleteTextures(1, &m_ResolveColorAttachment);
        if (m_ResolveEntityIDAttachment)
            glDeleteTextures(1, &m_ResolveEntityIDAttachment);
        m_ResolveFBO = 0;
        m_ResolveColorAttachment = 0;
        m_ResolveEntityIDAttachment = 0;
    }

    const bool msaa = m_Spec.samples > 1;

    glCreateFramebuffers(1, &m_RendererID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

    // --- Color attachment 0 (RGBA8) — skip for depth-only FBOs ---
    if (m_Spec.colorAttachment) {
        if (msaa) {
            glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &m_ColorAttachment);
            glTextureStorage2DMultisample(m_ColorAttachment, m_Spec.samples, GL_RGBA8,
                                          m_Spec.width, m_Spec.height, GL_TRUE);
            glNamedFramebufferTexture(m_RendererID, GL_COLOR_ATTACHMENT0, m_ColorAttachment, 0);
        } else {
            glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachment);
            glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Spec.width, m_Spec.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);
        }
    }

    // --- Color attachment 1: Entity ID (R32I integer texture) for mouse picking ---
    if (m_Spec.entityID && m_Spec.colorAttachment) {
        if (msaa) {
            glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &m_EntityIDAttachment);
            glTextureStorage2DMultisample(m_EntityIDAttachment, m_Spec.samples, GL_R32I,
                                          m_Spec.width, m_Spec.height, GL_TRUE);
            glNamedFramebufferTexture(m_RendererID, GL_COLOR_ATTACHMENT1, m_EntityIDAttachment, 0);
        } else {
            glCreateTextures(GL_TEXTURE_2D, 1, &m_EntityIDAttachment);
            glBindTexture(GL_TEXTURE_2D, m_EntityIDAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, m_Spec.width, m_Spec.height, 0, GL_RED_INTEGER, GL_INT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_EntityIDAttachment, 0);
        }

        GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, drawBuffers);
    }

    // --- No color attachment — depth-only FBO ---
    if (!m_Spec.colorAttachment) {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    // --- Depth-stencil attachment ---
    m_DepthIsTexture = m_Spec.depthAsTexture;
    if (m_Spec.depthAsTexture) {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, m_Spec.width, m_Spec.height, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
    } else {
        glCreateRenderbuffers(1, &m_DepthAttachment);
        if (msaa) {
            glNamedRenderbufferStorageMultisample(m_DepthAttachment, m_Spec.samples,
                                                  GL_DEPTH32F_STENCIL8,
                                                  m_Spec.width, m_Spec.height);
        } else {
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, m_Spec.width, m_Spec.height);
        }
        glNamedFramebufferRenderbuffer(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_RENDERBUFFER, m_DepthAttachment);
    }

    PULUO_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                       "Framebuffer is incomplete!");

    // --- Create resolve FBO for MSAA ---
    if (msaa && m_Spec.colorAttachment) {
        glCreateFramebuffers(1, &m_ResolveFBO);

        // Resolve color texture (regular GL_TEXTURE_2D, sampleable)
        glCreateTextures(GL_TEXTURE_2D, 1, &m_ResolveColorAttachment);
        glTextureStorage2D(m_ResolveColorAttachment, 1, GL_RGBA8, m_Spec.width, m_Spec.height);
        glTextureParameteri(m_ResolveColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_ResolveColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(m_ResolveFBO, GL_COLOR_ATTACHMENT0, m_ResolveColorAttachment, 0);

        // Resolve entity ID texture
        if (m_Spec.entityID) {
            glCreateTextures(GL_TEXTURE_2D, 1, &m_ResolveEntityIDAttachment);
            glTextureStorage2D(m_ResolveEntityIDAttachment, 1, GL_R32I, m_Spec.width, m_Spec.height);
            glTextureParameteri(m_ResolveEntityIDAttachment, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(m_ResolveEntityIDAttachment, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glNamedFramebufferTexture(m_ResolveFBO, GL_COLOR_ATTACHMENT1, m_ResolveEntityIDAttachment, 0);
        }

        PULUO_CORE_ASSERT(glCheckNamedFramebufferStatus(m_ResolveFBO, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                           "Resolve framebuffer is incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resolve() {
    if (m_Spec.samples <= 1) return;

    // Resolve color attachment 0
    glNamedFramebufferReadBuffer(m_RendererID, GL_COLOR_ATTACHMENT0);
    glNamedFramebufferDrawBuffer(m_ResolveFBO, GL_COLOR_ATTACHMENT0);
    glBlitNamedFramebuffer(m_RendererID, m_ResolveFBO,
        0, 0, m_Spec.width, m_Spec.height,
        0, 0, m_Spec.width, m_Spec.height,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve entity ID attachment (integer — must use GL_NEAREST)
    if (m_Spec.entityID) {
        glNamedFramebufferReadBuffer(m_RendererID, GL_COLOR_ATTACHMENT1);
        glNamedFramebufferDrawBuffer(m_ResolveFBO, GL_COLOR_ATTACHMENT1);
        glBlitNamedFramebuffer(m_RendererID, m_ResolveFBO,
            0, 0, m_Spec.width, m_Spec.height,
            0, 0, m_Spec.width, m_Spec.height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // Restore draw buffers on resolve FBO
    if (m_Spec.entityID) {
        GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glNamedFramebufferDrawBuffers(m_ResolveFBO, 2, bufs);
    }
}

uint32_t Framebuffer::GetColorAttachmentID() const {
    return (m_Spec.samples > 1) ? m_ResolveColorAttachment : m_ColorAttachment;
}

uint32_t Framebuffer::GetEntityIDAttachmentID() const {
    return (m_Spec.samples > 1) ? m_ResolveEntityIDAttachment : m_EntityIDAttachment;
}

int Framebuffer::ReadPixel(int attachmentIndex, int x, int y) const {
    // For MSAA, read from the resolved FBO
    uint32_t readFBO = (m_Spec.samples > 1) ? m_ResolveFBO : m_RendererID;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
    int pixelData = -1;
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return pixelData;
}

void Framebuffer::ClearEntityIDAttachment(int value) const {
    if (!m_EntityIDAttachment) return;
    glClearBufferiv(GL_COLOR, 1, &value);
}

} // namespace Puluo
