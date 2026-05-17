#pragma once

#include "puluo/renderer/RenderPass.h"

namespace Puluo {

// Depth pre-pass for SSAO, SSR, and water shore fade.
// Renders all opaque geometry (models, instanced meshes, terrain) into a depth-only FBO.
class DepthPrepassPass : public RenderPass {
public:
    DepthPrepassPass() : RenderPass("DepthPrepass") {}

    void Setup(RenderContext& ctx) override;
    void Execute(RenderContext& ctx) override;
    void Cleanup(RenderContext& ctx) override;
};

} // namespace Puluo
