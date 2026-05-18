#include "puluo/renderer/SSAOPass.h"
#include "puluo/renderer/RenderContext.h"
#include "puluo/renderer/SSAO.h"
#include "puluo/renderer/Framebuffer.h"
#include "puluo/renderer/Camera.h"

namespace Puluo {

void SSAOPass::Setup(RenderContext& ctx) {
    bool enabled = ctx.ssao && ctx.ssaoConfig && ctx.ssaoConfig->enabled
                && ctx.depthPrepassFB && ctx.emptyVAO != 0;
    SetEnabled(enabled);
}

void SSAOPass::Execute(RenderContext& ctx) {
    if (!IsEnabled()) return;

    // Sync UI config to SSAO object
    ctx.ssao->SetKernelSize(ctx.ssaoConfig->kernelSize);
    ctx.ssao->GetConfig().radius = ctx.ssaoConfig->radius;
    ctx.ssao->GetConfig().bias = ctx.ssaoConfig->bias;
    ctx.ssao->GetConfig().power = ctx.ssaoConfig->power;

    ctx.ssao->Generate(
        ctx.depthPrepassFB->GetDepthAttachmentID(),
        ctx.camera->GetProjectionMatrixUnjittered(),
        ctx.emptyVAO);
}

void SSAOPass::Cleanup(RenderContext& ctx) {
    // Nothing to clean up — SSAO unbinds its own FBOs internally
}

} // namespace Puluo
