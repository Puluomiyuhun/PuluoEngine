#include "puluo/renderer/PostProcessPass.h"
#include "puluo/renderer/RenderContext.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/Framebuffer.h"
#include "puluo/renderer/SSR.h"
#include "puluo/renderer/Camera.h"

#include <glad/gl.h>

namespace Puluo {

void PostProcessPass::Setup(RenderContext& ctx) {
    bool ssrActive = ctx.ssr && ctx.ssrConfig && ctx.ssrConfig->enabled && ctx.ssr->IsCreated();
    bool needPostProcess = ctx.fxaaEnabled || ssrActive;
    if (!needPostProcess || !ctx.sceneFB || !ctx.postProcessFB || !ctx.fxaaShader) {
        SetEnabled(false);
        return;
    }
    SetEnabled(true);
}

void PostProcessPass::Execute(RenderContext& ctx) {
    if (!IsEnabled()) return;

    // ---- SSR Generation ----
    bool ssrActive = ctx.ssr && ctx.ssrConfig && ctx.ssrConfig->enabled && ctx.ssr->IsCreated();
    if (ssrActive && ctx.depthPrepassFB) {
        auto& fbSpec = ctx.sceneFB->GetSpec();
        ctx.ssr->Generate(
            ctx.depthPrepassFB->GetDepthAttachmentID(),
            ctx.sceneFB->GetColorAttachmentID(),
            ctx.camera->GetProjectionMatrix(),
            ctx.camera->GetViewMatrix(),
            *ctx.ssrConfig,
            Vec2(static_cast<float>(fbSpec.width), static_cast<float>(fbSpec.height)),
            ctx.emptyVAO);
    }

    // ---- FXAA + SSR Composite ----
    ctx.postProcessFB->Bind();
    RenderCommand::SetDepthTest(false);
    RenderCommand::SetBlending(false);

    ctx.fxaaShader->Bind();
    ctx.fxaaShader->SetInt("uScreenTexture", 0);
    auto& fbSpec = ctx.sceneFB->GetSpec();
    ctx.fxaaShader->SetVec2("uInverseScreenSize",
        Vec2(1.0f / static_cast<float>(fbSpec.width),
             1.0f / static_cast<float>(fbSpec.height)));

    // Use TAA output if available, otherwise use sceneFB
    uint32_t sceneColor = ctx.taaOutputTexture ? ctx.taaOutputTexture
                                                : ctx.sceneFB->GetColorAttachmentID();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColor);

    // SSR composite
    ctx.fxaaShader->SetInt("uSSREnabled", ssrActive ? 1 : 0);
    if (ssrActive) {
        ctx.fxaaShader->SetInt("uSSRTexture", 10);
        ctx.ssr->BindTexture(10);
    }

    // Color grading
    ctx.fxaaShader->SetFloat("uSaturation", ctx.saturation);
    ctx.fxaaShader->SetFloat("uContrast", ctx.contrast);

    glBindVertexArray(ctx.emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    RenderCommand::SetDepthTest(true);
}

void PostProcessPass::Cleanup(RenderContext& ctx) {
    if (!IsEnabled()) return;
    ctx.postProcessFB->Unbind();
}

} // namespace Puluo
