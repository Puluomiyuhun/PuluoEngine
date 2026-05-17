#pragma once

#include "puluo/renderer/RenderPass.h"

namespace Puluo {

// Cascaded Shadow Map rendering pass.
// Renders all scene geometry into the CSM depth texture array.
class ShadowPass : public RenderPass {
public:
    ShadowPass() : RenderPass("ShadowPass") {}

    void Setup(RenderContext& ctx) override;
    void Execute(RenderContext& ctx) override;
    void Cleanup(RenderContext& ctx) override;

private:
    int m_SavedViewport[4] = {0, 0, 0, 0};
};

} // namespace Puluo
