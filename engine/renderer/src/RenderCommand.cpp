#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/VertexArray.h"

#include <glad/gl.h>

namespace Puluo {

void RenderCommand::Init() {
    // Reversed-Z: [0,1] depth range, near=1 far=0
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glClearDepth(0.0);
    glDepthFunc(GL_GEQUAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void RenderCommand::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void RenderCommand::SetClearColor(const Vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
}

void RenderCommand::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderCommand::ClearDepth() {
    glClear(GL_DEPTH_BUFFER_BIT);
}

void RenderCommand::DrawIndexed(const std::shared_ptr<VertexArray>& vao, uint32_t indexCount) {
    vao->Bind();
    uint32_t count = indexCount ? indexCount : vao->GetIndexBuffer()->GetCount();
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

void RenderCommand::DrawArrays(uint32_t vertexCount) {
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

void RenderCommand::DrawLines(const std::shared_ptr<VertexArray>& vao, uint32_t vertexCount) {
    vao->Bind();
    glDrawArrays(GL_LINES, 0, vertexCount);
}

void RenderCommand::DrawPatches(const std::shared_ptr<VertexArray>& vao, uint32_t vertexCount, int patchVertices) {
    vao->Bind();
    glPatchParameteri(GL_PATCH_VERTICES, patchVertices);
    glDrawArrays(GL_PATCHES, 0, vertexCount);
}

void RenderCommand::SetLineWidth(float width) {
    glLineWidth(width);
}

void RenderCommand::SetDepthTest(bool enabled) {
    if (enabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

void RenderCommand::SetBlending(bool enabled) {
    if (enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void RenderCommand::SetWireframe(bool enabled) {
    glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
}

void RenderCommand::DrawArraysInstanced(const std::shared_ptr<VertexArray>& vao,
                                         uint32_t vertexCount, uint32_t instanceCount) {
    vao->Bind();
    glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
}

void RenderCommand::SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor) {
    glBlendFunc(srcFactor, dstFactor);
}

void RenderCommand::SetDepthWrite(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void RenderCommand::SetCullFace(CullFace mode) {
    if (mode == CullFace::None) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        switch (mode) {
            case CullFace::Front: glCullFace(GL_FRONT); break;
            case CullFace::Back:  glCullFace(GL_BACK); break;
            case CullFace::FrontAndBack: glCullFace(GL_FRONT_AND_BACK); break;
            default: break;
        }
    }
}

void RenderCommand::SetPolygonOffset(bool enabled, float constant, float slope) {
    if (enabled) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(constant, slope);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void RenderCommand::DrawIndexedInstanced(const std::shared_ptr<VertexArray>& vao,
                                          uint32_t instanceCount, uint32_t indexCount) {
    if (!vao || instanceCount == 0) return;
    vao->Bind();
    auto ib = vao->GetIndexBuffer();
    if (!ib) return;
    uint32_t count = indexCount ? indexCount : ib->GetCount();
    if (count == 0) return;
    glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instanceCount);
}

} // namespace Puluo
