#pragma once

#include "puluo/core/Math.h"
#include <memory>
#include <cstdint>

namespace Puluo {

class VertexArray;

enum class CullFace { None, Front, Back, FrontAndBack };

class RenderCommand {
public:
    static void Init();

    static void SetViewport(int x, int y, int width, int height);
    static void SetClearColor(const Vec4& color);
    static void Clear();
    static void ClearDepth();

    static void DrawIndexed(const std::shared_ptr<VertexArray>& vao, uint32_t indexCount = 0);
    static void DrawArrays(uint32_t vertexCount);
    static void DrawLines(const std::shared_ptr<VertexArray>& vao, uint32_t vertexCount);
    static void DrawPatches(const std::shared_ptr<VertexArray>& vao, uint32_t vertexCount, int patchVertices = 4);
    static void SetLineWidth(float width);

    static void SetDepthTest(bool enabled);
    static void SetDepthWrite(bool enabled);
    static void SetBlending(bool enabled);
    static void SetWireframe(bool enabled);

    // Cull face control
    static void SetCullFace(CullFace mode);

    // Polygon offset (for shadow mapping bias)
    static void SetPolygonOffset(bool enabled, float constant = 0.0f, float slope = 0.0f);

    // Particle system support
    static void DrawArraysInstanced(const std::shared_ptr<VertexArray>& vao,
                                     uint32_t vertexCount, uint32_t instanceCount);
    static void SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor);

    // Instanced mesh support
    static void DrawIndexedInstanced(const std::shared_ptr<VertexArray>& vao,
                                      uint32_t instanceCount, uint32_t indexCount = 0);
};

} // namespace Puluo
