#pragma once

#include <string>

namespace Puluo {

struct RenderContext;

// Abstract base class for a render pass.
// Each pass encapsulates a distinct rendering stage (shadow, depth prepass, SSAO, etc.)
// with its own GL state setup and teardown.
class RenderPass {
public:
    explicit RenderPass(const std::string& name) : m_Name(name) {}
    virtual ~RenderPass() = default;

    // Setup GL state (bind FBO, set viewport, polygon offset, cull face, etc.)
    virtual void Setup(RenderContext& ctx) = 0;

    // Execute draw calls
    virtual void Execute(RenderContext& ctx) = 0;

    // Restore GL state (unbind FBO, reset polygon offset, etc.)
    virtual void Cleanup(RenderContext& ctx) = 0;

    // Convenience: runs Setup -> Execute -> Cleanup
    void Run(RenderContext& ctx) {
        if (!m_Enabled) return;
        Setup(ctx);
        Execute(ctx);
        Cleanup(ctx);
    }

    const std::string& GetName() const { return m_Name; }
    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

protected:
    std::string m_Name;
    bool m_Enabled = true;
};

} // namespace Puluo
