#pragma once

#include "puluo/renderer/RenderPass.h"

namespace Puluo {

// SSAO generation and blur pass.
// Requires depth prepass to have run first (uses depthPrepassFB depth attachment).
class SSAOPass : public RenderPass {
public:
    SSAOPass() : RenderPass("SSAO") {}

    void Setup(RenderContext& ctx) override;
    void Execute(RenderContext& ctx) override;
    void Cleanup(RenderContext& ctx) override;
};

} // namespace Puluo
