#include "puluo/renderer/ShadowPass.h"
#include "puluo/renderer/RenderContext.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/Renderer.h"
#include "puluo/renderer/CascadedShadowMap.h"
#include "puluo/renderer/InstancedMesh.h"
#include "puluo/renderer/Terrain.h"
#include "puluo/renderer/Model.h"
#include "puluo/core/Scene.h"

#include <glad/gl.h>

namespace Puluo {

void ShadowPass::Setup(RenderContext& ctx) {
    if (!ctx.csm || !ctx.csm->IsCreated() || !Renderer::s_ShadowEnabled) {
        SetEnabled(false);
        return;
    }
    SetEnabled(true);

    // Update cascade splits and light-space matrices
    ctx.csm->Update(*ctx.camera, ctx.shadowLightDir);

    // Save current viewport
    glGetIntegerv(GL_VIEWPORT, m_SavedViewport);

    // Enable depth bias to reduce shadow acne
    RenderCommand::SetPolygonOffset(true, ctx.csmConfig->depthBiasConstant, ctx.csmConfig->depthBiasSlope);

    // Cull front faces during shadow pass to prevent peter-panning
    RenderCommand::SetCullFace(CullFace::Front);

    // Shadow maps use standard (non-reversed) depth: near=0, far=1
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
}

void ShadowPass::Execute(RenderContext& ctx) {
    if (!IsEnabled()) return;

    for (uint32_t i = 0; i < ctx.csm->GetCascadeCount(); i++) {
        ctx.csm->BindCascade(i);

        Mat4 lightSpaceMat = ctx.csm->GetLightSpaceMatrix(i);

        // --- Models ---
        if (ctx.shadowModelShader) {
            ctx.shadowModelShader->Bind();
            ctx.shadowModelShader->SetMat4("uLightSpaceMatrix", lightSpaceMat);

            for (auto& obj : ctx.scene->GetObjects()) {
                if (obj.model && !obj.model->GetMeshes().empty()) {
                    ctx.shadowModelShader->SetMat4("uModel", obj.transform.ToMatrix());
                    const auto& materials = obj.model->GetMaterials();
                    for (auto& modelMesh : obj.model->GetMeshes()) {
                        if (modelMesh.materialIndex >= 0 &&
                            modelMesh.materialIndex < static_cast<int>(materials.size())) {
                            Renderer::BindAlphaMaskForDepth(ctx.shadowModelShader, materials[modelMesh.materialIndex]);
                        } else {
                            ctx.shadowModelShader->SetInt("uUseAlphaMask", 0);
                        }
                        modelMesh.mesh.Draw();
                    }
                }
            }
        }

        // --- Instanced meshes ---
        if (ctx.shadowModelInstancedShader && ctx.instancedMeshes && !ctx.instancedMeshes->empty()) {
            ctx.shadowModelInstancedShader->Bind();
            ctx.shadowModelInstancedShader->SetMat4("uLightSpaceMatrix", lightSpaceMat);
            for (auto& [idx, im] : *ctx.instancedMeshes) {
                im->DrawAllMeshesCulled();
            }
        }

        // --- Terrain ---
        // Terrain is single-sided, use back-face culling instead of front
        if (ctx.terrain && ctx.terrain->IsCreated() && ctx.shadowTerrainShader) {
            RenderCommand::SetCullFace(CullFace::Back);

            Vec3 terrainPos(0.0f);
            for (auto& obj : ctx.scene->GetObjects()) {
                if (obj.terrain.has_value()) { terrainPos = obj.transform.position; break; }
            }
            float halfSize = ctx.terrainParams->worldSize * 0.5f;
            Vec3 origin(-halfSize + terrainPos.x, terrainPos.y, -halfSize + terrainPos.z);

            ctx.shadowTerrainShader->Bind();
            ctx.shadowTerrainShader->SetMat4("uLightSpaceMatrix", lightSpaceMat);
            ctx.shadowTerrainShader->SetFloat("uTerrainSize", ctx.terrainParams->worldSize);
            ctx.shadowTerrainShader->SetVec3("uTerrainOrigin", origin);
            ctx.shadowTerrainShader->SetFloat("uHeightScale", ctx.terrainParams->heightScale);
            ctx.shadowTerrainShader->SetVec3("uCamPos", ctx.camera->GetPosition());

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ctx.terrain->GetHeightmapTexture());
            ctx.shadowTerrainShader->SetInt("uHeightmap", 0);

            RenderCommand::DrawPatches(ctx.terrain->GetPatchVAO(), ctx.terrain->GetPatchVertexCount());

            // Restore front-face culling for remaining shadow objects
            RenderCommand::SetCullFace(CullFace::Front);
        }
    }

    ctx.csm->Unbind();
}

void ShadowPass::Cleanup(RenderContext& ctx) {
    if (!IsEnabled()) return;

    RenderCommand::SetPolygonOffset(false);
    RenderCommand::SetCullFace(CullFace::Back);
    RenderCommand::SetViewport(m_SavedViewport[0], m_SavedViewport[1],
                               m_SavedViewport[2], m_SavedViewport[3]);

    // Restore reversed-Z state
    glClearDepth(0.0);
    glDepthFunc(GL_GEQUAL);
}

} // namespace Puluo
