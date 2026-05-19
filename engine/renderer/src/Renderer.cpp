#include "puluo/renderer/Renderer.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/Model.h"
#include "puluo/renderer/Cubemap.h"
#include "puluo/renderer/Terrain.h"
#include "puluo/renderer/CascadedShadowMap.h"
#include "puluo/renderer/SSAO.h"
#include <glad/gl.h>

namespace Puluo {

Renderer::SceneData Renderer::s_SceneData;
std::unique_ptr<UniformBuffer> Renderer::s_LightUBO;
const IBLMaps* Renderer::s_IBLMaps = nullptr;
float Renderer::s_IBLIntensity = 1.0f; 
const FogParams* Renderer::s_FogParams = nullptr;
const CascadedShadowMap* Renderer::s_CSM = nullptr;
bool Renderer::s_ShadowEnabled = true;
const SSAO* Renderer::s_SSAO = nullptr;
Vec2 Renderer::s_FramebufferSize{0.0f};
int Renderer::s_TerrainDebugMode = 0;    
Mesh Renderer::s_SkyboxCube;
uint32_t Renderer::s_DrawCallCount = 0;
uint32_t Renderer::s_CulledCount = 0;

void Renderer::Init() {
    RenderCommand::Init();
    // Create light UBO at binding point 0
    s_LightUBO = std::make_unique<UniformBuffer>(sizeof(LightBufferData), 0);
    s_SkyboxCube = Mesh::CreateCube();
}

void Renderer::Shutdown() {
    s_LightUBO.reset();
}

void Renderer::ResetFrameStats() {
    s_DrawCallCount = 0;
    s_CulledCount = 0;
}

void Renderer::IncrementDrawCall() { ++s_DrawCallCount; }
void Renderer::IncrementCulled()   { ++s_CulledCount; }
uint32_t Renderer::GetDrawCallCount() { return s_DrawCallCount; }
uint32_t Renderer::GetCulledCount()   { return s_CulledCount; }

void Renderer::SetIBLMaps(const IBLMaps* iblMaps) {
    s_IBLMaps = iblMaps;
}

void Renderer::SetIBLIntensity(float intensity) {
    s_IBLIntensity = intensity;
}

void Renderer::SetFogParams(const FogParams* fogParams) {
    s_FogParams = fogParams;
}

void Renderer::SetShadowMap(const CascadedShadowMap* csm) {
    s_CSM = csm;
    if (csm && csm->IsCreated()) {
        csm->BindShadowMapTexture(8);
    }
}

void Renderer::SetSSAOMap(const SSAO* ssao, const Vec2& fbSize) {
    s_SSAO = ssao;
    s_FramebufferSize = fbSize;
    if (ssao && ssao->IsCreated() && ssao->GetConfig().enabled) {
        ssao->BindTexture(9);
    }
}

void Renderer::SetTerrainDebugMode(int mode) {
    s_TerrainDebugMode = mode;
}

void Renderer::BeginScene(const CameraController& cameraCtrl, const LightManager& lights) {
    s_SceneData.viewProjection = cameraCtrl.GetViewProjection();
    s_SceneData.cameraPosition = cameraCtrl.GetPosition();

    // Upload light data
    LightBufferData lightData = lights.GetBufferData();
    s_LightUBO->SetData(&lightData, sizeof(LightBufferData));
}

void Renderer::BeginScene(const Camera& camera, const Mat4& viewMatrix, const LightManager& lights) {
    s_SceneData.viewProjection = camera.GetProjection() * viewMatrix;
    s_SceneData.cameraPosition = Vec3(0.0f); // TODO: extract from view matrix

    LightBufferData lightData = lights.GetBufferData();
    s_LightUBO->SetData(&lightData, sizeof(LightBufferData));
}

void Renderer::BeginScene(const CameraController& cameraCtrl) {
    s_SceneData.viewProjection = cameraCtrl.GetViewProjection();
    s_SceneData.cameraPosition = cameraCtrl.GetPosition();
}

void Renderer::EndScene() {
}

void Renderer::Submit(const std::shared_ptr<Shader>& shader,
                       const std::shared_ptr<VertexArray>& vao,
                       const Mat4& transform) {
    shader->Bind();
    shader->SetMat4("uViewProjection", s_SceneData.viewProjection);
    shader->SetMat4("uModel", transform);
    RenderCommand::DrawIndexed(vao);
}

void Renderer::Submit(const std::shared_ptr<Shader>& shader,
                       const Mesh& mesh,
                       const Mat4& transform) {
    shader->Bind();
    shader->SetMat4("uViewProjection", s_SceneData.viewProjection);
    shader->SetMat4("uModel", transform);
    mesh.Draw();
}

// Set per-frame uniforms that don't change between meshes/models.
// Call once after shader bind, before the model loop.
void Renderer::BindFrameUniforms(const std::shared_ptr<Shader>& shader) {
    shader->SetVec3("uCamPos", s_SceneData.cameraPosition);

    // Texture slot assignments (constant, could be set once but cheap)
    shader->SetInt("uAlbedoMap", 0);
    shader->SetInt("uNormalMap", 1);
    shader->SetInt("uMetallicMap", 2);
    shader->SetInt("uRoughnessMap", 3);
    shader->SetInt("uAOMap", 4);

    // IBL maps (slots 5, 6, 7)
    bool useIBL = s_IBLMaps != nullptr && s_IBLMaps->irradianceMap != nullptr;
    shader->SetInt("uUseIBL", useIBL ? 1 : 0);
    if (useIBL) {
        shader->SetInt("uIrradianceMap", 5);
        shader->SetInt("uPrefilterMap", 6);
        shader->SetInt("uBrdfLUT", 7);
        shader->SetFloat("uIBLIntensity", s_IBLIntensity);
        s_IBLMaps->irradianceMap->Bind(5);
        s_IBLMaps->prefilterMap->Bind(6);
        glBindTextureUnit(7, s_IBLMaps->brdfLUT);
    }

    // Fog parameters
    bool fogEnabled = s_FogParams != nullptr && s_FogParams->enabled;
    shader->SetInt("uFogEnabled", fogEnabled ? 1 : 0);
    if (fogEnabled) {
        shader->SetFloat("uFogDensity", s_FogParams->density);
        shader->SetFloat("uFogHeightFalloff", s_FogParams->heightFalloff);
        shader->SetFloat("uFogMaxOpacity", s_FogParams->maxOpacity);
        shader->SetVec3("uFogColor", s_FogParams->fogColor);
        shader->SetFloat("uFogStartDistance", s_FogParams->startDistance);
        shader->SetVec3("uFogDirInscatterColor", s_FogParams->directionalInscatteringColor);
        shader->SetFloat("uFogDirInscatterExp", s_FogParams->directionalInscatteringExponent);
        shader->SetFloat("uFogDirInscatterStartDist", s_FogParams->directionalInscatteringStartDistance);
        shader->SetVec3("uFogSunDirection", s_FogParams->sunDirection);
    }

    // Shadow map (slot 8) — must rebind every call; terrain clobbers this slot
    bool shadowEnabled = s_ShadowEnabled && s_CSM != nullptr && s_CSM->IsCreated();
    shader->SetInt("uShadowEnabled", shadowEnabled ? 1 : 0);
    if (shadowEnabled) {
        shader->SetInt("uShadowMap", 8);
        s_CSM->BindShadowMapTexture(8);
        shader->SetMat4Array("uLightSpaceMatrices[0]",
            s_CSM->GetLightSpaceMatrices().data(), s_CSM->GetCascadeCount());
        shader->SetFloatArray("uCascadeSplits[0]",
            s_CSM->GetCascadeSplits().data(), s_CSM->GetCascadeCount());
        shader->SetInt("uCascadeCount", static_cast<int>(s_CSM->GetCascadeCount()));
        shader->SetFloat("uShadowNormalBias", s_CSM->GetConfig().normalBias);
        shader->SetFloat("uShadowIntensity", s_CSM->GetConfig().shadowIntensity);
    }

    // SSAO (slot 9) — must rebind every call; terrain clobbers this slot
    bool ssaoEnabled = s_SSAO != nullptr && s_SSAO->IsCreated() && s_SSAO->GetConfig().enabled;
    shader->SetInt("uSSAOEnabled", ssaoEnabled ? 1 : 0);
    if (ssaoEnabled) {
        shader->SetInt("uSSAOMap", 9);
        s_SSAO->BindTexture(9);
        shader->SetVec2("uScreenSize", s_FramebufferSize);
    }

    // Alpha mask (slot 10)
    shader->SetInt("uMaskMap", 10);
}

void Renderer::BindPBRMaterial(const std::shared_ptr<Shader>& shader,
                                const PBRMaterialData& material) {
    // Scalar fallbacks
    shader->SetVec3("uAlbedo", material.albedo);
    shader->SetFloat("uMetallic", material.metallic);
    shader->SetFloat("uRoughness", material.roughness);
    shader->SetFloat("uAO", material.ao);

    // Texture flags and bindings
    bool hasAlbedo = material.albedoMap != nullptr;
    bool hasNormal = material.normalMap != nullptr;
    bool hasMetallic = material.metallicMap != nullptr;
    bool hasRoughness = material.roughnessMap != nullptr;
    bool hasAO = material.aoMap != nullptr;

    shader->SetInt("uUseAlbedoMap", hasAlbedo ? 1 : 0);
    shader->SetInt("uUseNormalMap", hasNormal ? 1 : 0);
    shader->SetInt("uUseMetallicMap", hasMetallic ? 1 : 0);
    shader->SetInt("uUseRoughnessMap", hasRoughness ? 1 : 0);
    shader->SetInt("uUseAOMap", hasAO ? 1 : 0);

    if (hasAlbedo)    material.albedoMap->Bind(0);
    if (hasNormal)    material.normalMap->Bind(1);
    if (hasMetallic)  material.metallicMap->Bind(2);
    if (hasRoughness) material.roughnessMap->Bind(3);
    if (hasAO)        material.aoMap->Bind(4);

    // Alpha mask
    bool hasMask = material.useAlphaMask && material.maskMap != nullptr;
    shader->SetInt("uUseAlphaMask", hasMask ? 1 : 0);
    if (hasMask) {
        material.maskMap->Bind(10);
        shader->SetFloat("uAlphaCutoff", material.alphaCutoff);
    }

    // Subsurface scattering
    shader->SetInt("uUseSSS", material.useSSS ? 1 : 0);
    if (material.useSSS) {
        shader->SetVec3("uSSSColor", material.sssColor);
        shader->SetFloat("uSSSStrength", material.sssStrength);
    }
}

void Renderer::SubmitModel(const std::shared_ptr<Shader>& pbrShader,
                            const Model& model,
                            const Mat4& transform,
                            int entityID) {
    pbrShader->Bind();
    pbrShader->SetMat4("uViewProjection", s_SceneData.viewProjection);
    BindFrameUniforms(pbrShader);

    pbrShader->SetMat4("uModel", transform);
    pbrShader->SetInt("uEntityID", entityID);

    Mat3 normalMatrix = glm::transpose(glm::inverse(Mat3(transform)));
    pbrShader->SetMat3("uNormalMatrix", normalMatrix);

    const auto& meshes = model.GetMeshes();
    const auto& materials = model.GetMaterials();

    for (const auto& modelMesh : meshes) {
        if (modelMesh.materialIndex >= 0 &&
            modelMesh.materialIndex < static_cast<int>(materials.size())) {
            BindPBRMaterial(pbrShader, materials[modelMesh.materialIndex]);

            // SSS materials need two-sided rendering
            bool needTwoSided = materials[modelMesh.materialIndex].useSSS;
            if (needTwoSided) glDisable(GL_CULL_FACE);
            modelMesh.mesh.Draw();
            if (needTwoSided) glEnable(GL_CULL_FACE);
        } else {
            modelMesh.mesh.Draw();
        }
    }
}

void Renderer::BindAlphaMaskForDepth(const std::shared_ptr<Shader>& depthShader,
                                      const PBRMaterialData& material) {
    bool hasMask = material.useAlphaMask && material.maskMap != nullptr;
    depthShader->SetInt("uUseAlphaMask", hasMask ? 1 : 0);
    if (hasMask) {
        depthShader->SetInt("uMaskMap", 0);
        material.maskMap->Bind(0);
        depthShader->SetFloat("uAlphaCutoff", material.alphaCutoff);
    }
}

void Renderer::RenderSkybox(const std::shared_ptr<Shader>& skyboxShader,
                             const CameraController& cameraCtrl) {
    if (!s_IBLMaps || !s_IBLMaps->envCubemap) return;

    // Render skybox as last (depth test <=, no depth write)
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);  // Skybox cube is viewed from inside

    skyboxShader->Bind();
    skyboxShader->SetMat4("uProjection", cameraCtrl.GetProjectionMatrix());
    skyboxShader->SetMat4("uView", cameraCtrl.GetViewMatrix());
    skyboxShader->SetInt("uEnvironmentMap", 0);
    s_IBLMaps->envCubemap->Bind(0);

    s_SkyboxCube.Draw();

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_GREATER);
}

void Renderer::RenderAtmosphere(const std::shared_ptr<Shader>& atmosphereShader,
                                  const CameraController& cameraCtrl,
                                  const AtmosphereParams& params) {
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);  // Atmosphere cube is viewed from inside

    atmosphereShader->Bind();
    atmosphereShader->SetMat4("uProjection", cameraCtrl.GetProjectionMatrix());
    atmosphereShader->SetMat4("uView", cameraCtrl.GetViewMatrix());
    atmosphereShader->SetVec3("uSunDirection", params.sunDirection);
    atmosphereShader->SetFloat("uSunIntensity", params.sunIntensity);
    atmosphereShader->SetFloat("uTurbidity", params.turbidity);
    atmosphereShader->SetVec3("uRayleighCoeff", params.rayleighCoeff);
    atmosphereShader->SetFloat("uMieCoeff", params.mieCoeff);
    atmosphereShader->SetFloat("uMieDirectionG", params.mieDirectionG);
    atmosphereShader->SetInt("uLinearOutput", 0);

    // Pass fog params to atmosphere shader for horizon blending
    bool fogEnabled = s_FogParams != nullptr && s_FogParams->enabled;
    atmosphereShader->SetInt("uFogEnabled", fogEnabled ? 1 : 0);
    if (fogEnabled) {
        atmosphereShader->SetFloat("uFogDensity", s_FogParams->density);
        atmosphereShader->SetFloat("uFogHeightFalloff", s_FogParams->heightFalloff);
        atmosphereShader->SetFloat("uFogMaxOpacity", s_FogParams->maxOpacity);
        atmosphereShader->SetVec3("uFogColor", s_FogParams->fogColor);
        atmosphereShader->SetVec3("uFogDirInscatterColor", s_FogParams->directionalInscatteringColor);
        atmosphereShader->SetFloat("uFogDirInscatterExp", s_FogParams->directionalInscatteringExponent);
        atmosphereShader->SetVec3("uFogSunDirection", s_FogParams->sunDirection);
    }

    s_SkyboxCube.Draw();

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_GREATER);
}

void Renderer::RenderClouds(const std::shared_ptr<Shader>& cloudShader,
                              const CameraController& cameraCtrl,
                              const CloudParams& params,
                              const AtmosphereParams& atmoParams,
                              uint32_t baseNoiseTexID,
                              uint32_t detailNoiseTexID,
                              float time) {
    if (!params.enabled) return;

    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);  // Cloud cube is viewed from inside
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // pre-multiplied alpha

    cloudShader->Bind();

    // Camera matrices (same as atmosphere: projection + rotation-only view)
    cloudShader->SetMat4("uProjection", cameraCtrl.GetProjectionMatrix());
    cloudShader->SetMat4("uView", cameraCtrl.GetViewMatrix());
    cloudShader->SetVec3("uCamPos", cameraCtrl.GetPosition()); 

    // Sun
    cloudShader->SetVec3("uSunDirection", atmoParams.sunDirection);
    cloudShader->SetFloat("uSunIntensity", atmoParams.sunIntensity);

    // Cloud parameters
    cloudShader->SetFloat("uCloudBottom", params.cloudLayerBottom);
    cloudShader->SetFloat("uCloudThickness", params.cloudLayerThickness);
    cloudShader->SetFloat("uCoverage", params.coverage);
    cloudShader->SetFloat("uCloudDensity", params.density);
    cloudShader->SetFloat("uBaseScale", params.baseScale);
    cloudShader->SetFloat("uDetailScale", params.detailScale);
    cloudShader->SetFloat("uTime", time);
    cloudShader->SetFloat("uWindSpeed", params.windSpeed);
    cloudShader->SetVec3("uWindDirection", params.windDirection);

    // Lighting
    cloudShader->SetFloat("uPhaseG", params.phaseG);
    cloudShader->SetFloat("uPowderStrength", params.powderStrength);
    cloudShader->SetVec3("uAmbientColor", params.ambientColor);
    cloudShader->SetFloat("uAmbientStrength", params.ambientStrength);

    // Bind 3D noise textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, baseNoiseTexID);
    cloudShader->SetInt("uBaseNoise", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, detailNoiseTexID);
    cloudShader->SetInt("uDetailNoise", 1);

    s_SkyboxCube.Draw();

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_GREATER);
}

void Renderer::RenderTerrain(const std::shared_ptr<Shader>& terrainShader,
                               const CameraController& cameraCtrl,
                               const Terrain& terrain,
                               const TerrainParams& params,
                               const TerrainMaterial& material,
                               const AtmosphereParams& atmoParams,
                               const Vec3& terrainPosition) {
    if (!terrain.IsCreated()) return;

    float halfSize = params.worldSize * 0.5f;
    Vec3 origin(-halfSize + terrainPosition.x, terrainPosition.y, -halfSize + terrainPosition.z);

    terrainShader->Bind();

    terrainShader->SetMat4("uViewProjection", cameraCtrl.GetViewProjection());
    terrainShader->SetVec3("uCamPos", cameraCtrl.GetPosition());

    // Terrain params
    terrainShader->SetFloat("uTerrainSize", params.worldSize);
    terrainShader->SetVec3("uTerrainOrigin", origin);
    terrainShader->SetFloat("uHeightScale", params.heightScale);
    terrainShader->SetFloat("uUVScale", params.uvScale);

    // Heightmap texture (slot 0)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, terrain.GetHeightmapTexture());
    terrainShader->SetInt("uHeightmap", 0);

    // Sun lighting (from atmosphere) — apply atmospheric extinction + night fade
    float sunBrightness = atmoParams.ComputeSunBrightness();
    Vec3 sunTransmittance = atmoParams.ComputeSunTransmittanceColor();
    // At high sun, transmittance color fades out (highSunFade in ComputeSunTransmittanceColor),
    // but we still want white-ish sunlight at noon, so blend back toward white based on brightness.
    Vec3 sunColor = glm::mix(sunTransmittance, Vec3(1.0f), glm::smoothstep(0.0f, 0.5f, atmoParams.sunDirection.y));
    sunColor *= sunBrightness;

    terrainShader->SetVec3("uSunDirection", atmoParams.sunDirection);
    terrainShader->SetVec3("uSunColor", sunColor);
    terrainShader->SetFloat("uSunIntensity", atmoParams.sunIntensity);

    // ---- Three-layer material textures (slots 1-9) ----
    auto bindLayer = [&](const TerrainLayerMaterial& layer,
                         const char* albedoName, const char* normalName, const char* roughnessName,
                         const char* hasAlbedo, const char* hasNormal, const char* hasRoughness,
                         int slotBase) {
        terrainShader->SetInt(hasAlbedo, layer.albedoTex ? 1 : 0);
        if (layer.albedoTex) {
            glActiveTexture(GL_TEXTURE0 + slotBase);
            glBindTexture(GL_TEXTURE_2D, layer.albedoTex);
            terrainShader->SetInt(albedoName, slotBase);
        }
        terrainShader->SetInt(hasNormal, layer.normalTex ? 1 : 0);
        if (layer.normalTex) {
            glActiveTexture(GL_TEXTURE0 + slotBase + 1);
            glBindTexture(GL_TEXTURE_2D, layer.normalTex);
            terrainShader->SetInt(normalName, slotBase + 1);
        }
        terrainShader->SetInt(hasRoughness, layer.roughnessTex ? 1 : 0);
        if (layer.roughnessTex) {
            glActiveTexture(GL_TEXTURE0 + slotBase + 2);
            glBindTexture(GL_TEXTURE_2D, layer.roughnessTex);
            terrainShader->SetInt(roughnessName, slotBase + 2);
        }
    };

    // Lower layer: slots 1-3
    bindLayer(material.lower,
              "uLowerAlbedo", "uLowerNormal", "uLowerRoughness",
              "uHasLowerAlbedo", "uHasLowerNormal", "uHasLowerRoughness", 1);
    // Upper layer: slots 4-6
    bindLayer(material.upper,
              "uUpperAlbedo", "uUpperNormal", "uUpperRoughness",
              "uHasUpperAlbedo", "uHasUpperNormal", "uHasUpperRoughness", 4);
    // Slope layer: slots 7-9
    bindLayer(material.slope,
              "uSlopeAlbedo", "uSlopeNormal", "uSlopeRoughness",
              "uHasSlopeAlbedo", "uHasSlopeNormal", "uHasSlopeRoughness", 7);

    // Blend parameters
    terrainShader->SetFloat("uHeightThreshold", material.heightThreshold);
    terrainShader->SetFloat("uSlopeThreshold", material.slopeThreshold);
    terrainShader->SetFloat("uBlendSharpness", material.blendSharpness);

    // Per-layer normal strength
    terrainShader->SetFloat("uLowerNormalStrength", material.lower.normalStrength);
    terrainShader->SetFloat("uUpperNormalStrength", material.upper.normalStrength);
    terrainShader->SetFloat("uSlopeNormalStrength", material.slope.normalStrength);

    // Splat map (slot 15)
    bool useSplatMap = terrain.HasSplatMap();
    terrainShader->SetInt("uUseSplatMap", useSplatMap ? 1 : 0);
    if (useSplatMap) {
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, terrain.GetSplatMapTexture());
        terrainShader->SetInt("uSplatMap", 15);
    }

    terrainShader->SetFloat("uRoughness", 0.85f);
    terrainShader->SetFloat("uMetallic", 0.0f);

    // Fog
    if (s_FogParams) {
        terrainShader->SetInt("uFogEnabled", 1);
        terrainShader->SetFloat("uFogDensity", s_FogParams->density);
        terrainShader->SetFloat("uFogHeightFalloff", s_FogParams->heightFalloff);
        terrainShader->SetFloat("uFogMaxOpacity", s_FogParams->maxOpacity);
        terrainShader->SetVec3("uFogColor", s_FogParams->fogColor);
        terrainShader->SetFloat("uFogStartDistance", s_FogParams->startDistance);
        terrainShader->SetVec3("uFogDirInscatterColor", s_FogParams->directionalInscatteringColor);
        terrainShader->SetFloat("uFogDirInscatterExp", s_FogParams->directionalInscatteringExponent);
        terrainShader->SetFloat("uFogDirInscatterStartDist", s_FogParams->directionalInscatteringStartDistance);
        terrainShader->SetVec3("uFogSunDirection", s_FogParams->sunDirection);
    } else {
        terrainShader->SetInt("uFogEnabled", 0);
    }

    // Shadow map (slot 10) — slots 0-9 used by heightmap + 3 material layers
    bool shadowEnabled = s_ShadowEnabled && s_CSM != nullptr && s_CSM->IsCreated();
    terrainShader->SetInt("uShadowEnabled", shadowEnabled ? 1 : 0);
    if (shadowEnabled) {
        terrainShader->SetInt("uShadowMap", 10);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D_ARRAY, s_CSM->GetTextureArrayID());
        terrainShader->SetMat4Array("uLightSpaceMatrices[0]",
            s_CSM->GetLightSpaceMatrices().data(), s_CSM->GetCascadeCount());
        terrainShader->SetFloatArray("uCascadeSplits[0]",
            s_CSM->GetCascadeSplits().data(), s_CSM->GetCascadeCount());
        terrainShader->SetInt("uCascadeCount", static_cast<int>(s_CSM->GetCascadeCount()));
        terrainShader->SetFloat("uShadowNormalBias", s_CSM->GetConfig().normalBias);
        terrainShader->SetFloat("uShadowIntensity", s_CSM->GetConfig().shadowIntensity);
    }

    // SSAO (slot 11)
    bool ssaoEnabled = s_SSAO != nullptr && s_SSAO->IsCreated() && s_SSAO->GetConfig().enabled;
    terrainShader->SetInt("uSSAOEnabled", ssaoEnabled ? 1 : 0);
    if (ssaoEnabled) {
        terrainShader->SetInt("uSSAOMap", 11);
        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_2D, s_SSAO->GetBlurredTexture());
        terrainShader->SetVec2("uScreenSize", s_FramebufferSize);
    }

    // Debug mode
    terrainShader->SetInt("uDebugMode", s_TerrainDebugMode);

    // IBL (slots 12-14)
    bool useIBL = s_IBLMaps != nullptr && s_IBLMaps->irradianceMap != nullptr;
    terrainShader->SetInt("uUseIBL", useIBL ? 1 : 0);
    if (useIBL) {
        terrainShader->SetInt("uIrradianceMap", 12);
        terrainShader->SetInt("uPrefilterMap", 13);
        terrainShader->SetInt("uBrdfLUT", 14);
        terrainShader->SetFloat("uIBLIntensity", s_IBLIntensity);
        s_IBLMaps->irradianceMap->Bind(12);
        s_IBLMaps->prefilterMap->Bind(13);
        glBindTextureUnit(14, s_IBLMaps->brdfLUT);
    }

    RenderCommand::DrawPatches(terrain.GetPatchVAO(), terrain.GetPatchVertexCount());
}

} // namespace Puluo
