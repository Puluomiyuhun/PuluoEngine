#pragma once

#include "puluo/renderer/RenderPass.h"

namespace Puluo {

// Post-process pass: SSR generation + FXAA + SSR composite.
// Reads from sceneFB, writes to postProcessFB.
class PostProcessPass : public RenderPass {
public:
    PostProcessPass() : RenderPass("PostProcess") {}

    void Setup(RenderContext& ctx) override;
    void Execute(RenderContext& ctx) override;
    void Cleanup(RenderContext& ctx) override;
};

} // namespace Puluo
