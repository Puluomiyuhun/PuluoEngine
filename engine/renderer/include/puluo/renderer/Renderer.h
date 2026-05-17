#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/Camera.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/VertexArray.h"
#include "puluo/renderer/Mesh.h"
#include "puluo/renderer/Light.h"
#include "puluo/renderer/Buffer.h"
#include "puluo/renderer/IBL.h"
#include "puluo/renderer/Atmosphere.h"
#include "puluo/renderer/Fog.h"
#include "puluo/renderer/Cloud.h"
#include "puluo/renderer/Terrain.h"
#include "puluo/renderer/CascadedShadowMap.h"
#include <memory>
#include <cstdint>

namespace Puluo {

class Model;
class SSAO;
struct PBRMaterialData;

class Renderer {
public:
    static void Init();
    static void Shutdown();

    static void BeginScene(const CameraController& cameraCtrl, const LightManager& lights);
    static void BeginScene(const Camera& camera, const Mat4& viewMatrix, const LightManager& lights);
    // Simpler overloads (no lights, backward compatible)
    static void BeginScene(const CameraController& cameraCtrl);

    static void EndScene();

    // Set IBL maps for PBR rendering (slots 5, 6, 7)
    static void SetIBLMaps(const IBLMaps* iblMaps);
    static void SetIBLIntensity(float intensity);

    // Set fog parameters for PBR rendering
    static void SetFogParams(const FogParams* fogParams);

    // Set CSM shadow map for PBR/terrain rendering (slot 8)
    static void SetShadowMap(const CascadedShadowMap* csm);

    // Runtime toggle for CSM shadows
    static bool s_ShadowEnabled;

    // Set SSAO texture for PBR/terrain rendering (slot 9)
    static void SetSSAOMap(const SSAO* ssao, const Vec2& framebufferSize);

    // Set terrain debug visualization mode (0=off, 1=shadow, 2=SSAO, 3=NdotL, 4=ambient, 5=sunColor)
    static void SetTerrainDebugMode(int mode);
    static int s_TerrainDebugMode;

    static void Submit(const std::shared_ptr<Shader>& shader,
                       const std::shared_ptr<VertexArray>& vao,
                       const Mat4& transform = Mat4(1.0f));

    static void Submit(const std::shared_ptr<Shader>& shader,
                       const Mesh& mesh,
                       const Mat4& transform = Mat4(1.0f));

    // PBR model rendering
    static void SubmitModel(const std::shared_ptr<Shader>& pbrShader,
                            const Model& model,
                            const Mat4& transform = Mat4(1.0f),
                            int entityID = -1);

    // Bind PBR material data to shader (per-mesh material properties only)
    static void BindPBRMaterial(const std::shared_ptr<Shader>& shader,
                                const PBRMaterialData& material);

    // Bind alpha mask for depth-only passes (shadow, SSAO prepass)
    static void BindAlphaMaskForDepth(const std::shared_ptr<Shader>& depthShader,
                                      const PBRMaterialData& material);

    // Bind per-frame uniforms (camera, IBL, fog, shadow) — call once per shader per frame
    static void BindFrameUniforms(const std::shared_ptr<Shader>& shader);

    // Render skybox with currently set IBL environment map
    static void RenderSkybox(const std::shared_ptr<Shader>& skyboxShader,
                             const CameraController& cameraCtrl);

    // Render procedural atmosphere sky
    static void RenderAtmosphere(const std::shared_ptr<Shader>& atmosphereShader,
                                  const CameraController& cameraCtrl,
                                  const AtmosphereParams& params);

    // Render volumetric clouds
    static void RenderClouds(const std::shared_ptr<Shader>& cloudShader,
                              const CameraController& cameraCtrl,
                              const CloudParams& params,
                              const AtmosphereParams& atmoParams,
                              uint32_t baseNoiseTexID,
                              uint32_t detailNoiseTexID,
                              float time);

    // Render terrain with tessellation
    static void RenderTerrain(const std::shared_ptr<Shader>& terrainShader,
                               const CameraController& cameraCtrl,
                               const Terrain& terrain,
                               const TerrainParams& params,
                               const TerrainMaterial& material,
                               const AtmosphereParams& atmoParams,
                               const Vec3& terrainPosition = Vec3(0.0f));

private:
    struct SceneData {
        Mat4 viewProjection{1.0f};
        Vec3 cameraPosition{0.0f};
    };
    static SceneData s_SceneData;
    static std::unique_ptr<UniformBuffer> s_LightUBO;
    static const IBLMaps* s_IBLMaps;
    static float s_IBLIntensity;
    static const FogParams* s_FogParams;
    static const CascadedShadowMap* s_CSM;
    static const SSAO* s_SSAO;
    static Vec2 s_FramebufferSize;
    static Mesh s_SkyboxCube;
};

} // namespace Puluo
