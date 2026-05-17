#pragma once

#include <cstdint>

namespace Puluo {

struct FramebufferSpec {
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t samples = 1;
    bool entityID = false;        // Create R32I attachment for mouse picking
    bool depthAsTexture = false;  // true: depth as Texture2D (sampleable); false: Renderbuffer
    bool colorAttachment = true;  // false: depth-only FBO (no color attachment)
};

class Framebuffer {
public:
    Framebuffer(const FramebufferSpec& spec);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void Bind() const;
    void Unbind() const;

    void Resize(uint32_t width, uint32_t height);

    uint32_t GetColorAttachmentID() const { return m_ColorAttachment; }
    uint32_t GetDepthAttachmentID() const { return m_DepthAttachment; }
    uint32_t GetRendererID() const { return m_RendererID; }
    const FramebufferSpec& GetSpec() const { return m_Spec; }

    // Entity ID picking support
    int ReadPixel(int attachmentIndex, int x, int y) const;
    void ClearEntityIDAttachment(int value) const;

private:
    uint32_t m_RendererID = 0;
    uint32_t m_ColorAttachment = 0;
    uint32_t m_EntityIDAttachment = 0;
    uint32_t m_DepthAttachment = 0;
    bool m_DepthIsTexture = false;
    FramebufferSpec m_Spec;

    void Invalidate();
};

} // namespace Puluo
