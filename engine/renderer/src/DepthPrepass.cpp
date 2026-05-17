#include "puluo/renderer/DepthPrepass.h"
#include "puluo/renderer/RenderContext.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/Renderer.h"
#include "puluo/renderer/Framebuffer.h"
#include "puluo/renderer/InstancedMesh.h"
#include "puluo/renderer/Terrain.h"
#include "puluo/renderer/Model.h"
#include "puluo/core/Scene.h"

#include <glad/gl.h>

namespace Puluo {

void DepthPrepassPass::Setup(RenderContext& ctx) {
    // Depth prepass is needed for SSAO, SSR, or water shore fade
    bool needPrepass = (ctx.ssao && ctx.ssaoConfig && ctx.ssaoConfig->enabled)
                     || (ctx.ssrConfig && ctx.ssrConfig->enabled)
                     || ctx.hasWaterObjects;
    if (!needPrepass || !ctx.depthPrepassFB) {
        SetEnabled(false);
        return;
    }
    SetEnabled(true);

    ctx.depthPrepassFB->Bind();
    RenderCommand::ClearDepth();
}

void DepthPrepassPass::Execute(RenderContext& ctx) {
    if (!IsEnabled()) return;

    // --- Models (depth only, with alpha mask) ---
    if (ctx.depthPrepassModelShader) {
        ctx.depthPrepassModelShader->Bind();
        ctx.depthPrepassModelShader->SetMat4("uViewProjection", ctx.camera->GetViewProjection());

        for (auto& obj : ctx.scene->GetObjects()) {
            if (obj.model && !obj.model->GetMeshes().empty()) {
                ctx.depthPrepassModelShader->SetMat4("uModel", obj.transform.ToMatrix());
                const auto& materials = obj.model->GetMaterials();
                for (auto& modelMesh : obj.model->GetMeshes()) {
                    if (modelMesh.materialIndex >= 0 &&
                        modelMesh.materialIndex < static_cast<int>(materials.size())) {
                        Renderer::BindAlphaMaskForDepth(ctx.depthPrepassModelShader, materials[modelMesh.materialIndex]);
                    } else {
                        ctx.depthPrepassModelShader->SetInt("uUseAlphaMask", 0);
                    }
                    modelMesh.mesh.Draw();
                }
            }
        }
    }

    // --- Instanced meshes (depth only, culled) ---
    if (ctx.depthPrepassInstancedShader && ctx.instancedMeshes && !ctx.instancedMeshes->empty()) {
        ctx.depthPrepassInstancedShader->Bind();
        ctx.depthPrepassInstancedShader->SetMat4("uViewProjection", ctx.camera->GetViewProjection());
        for (auto& [idx, im] : *ctx.instancedMeshes) {
            im->DrawAllMeshesCulled();
        }
    }

    // --- Terrain (depth only, with tessellation) ---
    if (ctx.terrain && ctx.terrain->IsCreated() && ctx.depthPrepassTerrainShader) {
        Vec3 terrainPos(0.0f);
        for (auto& obj : ctx.scene->GetObjects()) {
            if (obj.terrain.has_value()) { terrainPos = obj.transform.position; break; }
        }
        float halfSize = ctx.terrainParams->worldSize * 0.5f;
        Vec3 origin(-halfSize + terrainPos.x, terrainPos.y, -halfSize + terrainPos.z);

        ctx.depthPrepassTerrainShader->Bind();
        ctx.depthPrepassTerrainShader->SetMat4("uViewProjection", ctx.camera->GetViewProjection());
        ctx.depthPrepassTerrainShader->SetFloat("uTerrainSize", ctx.terrainParams->worldSize);
        ctx.depthPrepassTerrainShader->SetVec3("uTerrainOrigin", origin);
        ctx.depthPrepassTerrainShader->SetFloat("uHeightScale", ctx.terrainParams->heightScale);
        ctx.depthPrepassTerrainShader->SetVec3("uCamPos", ctx.camera->GetPosition());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx.terrain->GetHeightmapTexture());
        ctx.depthPrepassTerrainShader->SetInt("uHeightmap", 0);

        RenderCommand::DrawPatches(ctx.terrain->GetPatchVAO(), ctx.terrain->GetPatchVertexCount());
    }
}

void DepthPrepassPass::Cleanup(RenderContext& ctx) {
    if (!IsEnabled()) return;

    ctx.depthPrepassFB->Unbind();
}

} // namespace Puluo
