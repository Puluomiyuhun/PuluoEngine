#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/Camera.h"
#include "puluo/renderer/Light.h"
#include "puluo/renderer/Frustum.h"
#include <memory>
#include <cstdint>

namespace Puluo {

// Forward declarations
class Framebuffer;
class Shader;
class CascadedShadowMap;
class SSAO;
class SSR;
class Scene;
class Terrain;
class InstancedMesh;
struct CSMConfig;
struct SSAOConfig;
struct SSRConfig;
struct AtmosphereParams;
struct FogParams;
struct TerrainParams;
struct TerrainMaterial;

// Per-frame rendering context shared across all RenderPasses.
// Populated once per frame by the application, consumed by each Pass.
struct RenderContext {
    // Camera
    const CameraController* camera = nullptr;
    Frustum frustum;

    // Lights
    const LightManager* lights = nullptr;
    Vec3 shadowLightDir{0.0f, -1.0f, 0.0f};  // Direction toward main directional light

    // Scene
    const Scene* scene = nullptr;

    // Time
    float time = 0.0f;
    float deltaTime = 0.0f;

    // Viewport
    Vec2 viewportSize{0.0f};

    // Framebuffers (owned by app, referenced here)
    Framebuffer* sceneFB = nullptr;
    Framebuffer* depthPrepassFB = nullptr;
    Framebuffer* postProcessFB = nullptr;

    // Shadow
    CascadedShadowMap* csm = nullptr;
    const CSMConfig* csmConfig = nullptr;

    // SSAO
    SSAO* ssao = nullptr;
    const SSAOConfig* ssaoConfig = nullptr;

    // SSR
    SSR* ssr = nullptr;
    const SSRConfig* ssrConfig = nullptr;

    // Terrain
    const Terrain* terrain = nullptr;
    const TerrainParams* terrainParams = nullptr;
    const TerrainMaterial* terrainMaterial = nullptr;

    // Atmosphere
    const AtmosphereParams* atmosphereParams = nullptr;
    bool useAtmosphere = false;

    // Fog
    const FogParams* fogParams = nullptr;

    // Shaders (shared, owned by app)
    std::shared_ptr<Shader> pbrShader;
    std::shared_ptr<Shader> pbrInstancedShader;
    std::shared_ptr<Shader> terrainShader;
    std::shared_ptr<Shader> shadowModelShader;
    std::shared_ptr<Shader> shadowModelInstancedShader;
    std::shared_ptr<Shader> shadowTerrainShader;
    std::shared_ptr<Shader> depthPrepassModelShader;
    std::shared_ptr<Shader> depthPrepassInstancedShader;
    std::shared_ptr<Shader> depthPrepassTerrainShader;
    std::shared_ptr<Shader> fxaaShader;
    std::shared_ptr<Shader> skyboxShader;
    std::shared_ptr<Shader> atmosphereShader;
    std::shared_ptr<Shader> cloudShader;
    std::shared_ptr<Shader> lineShader;
    std::shared_ptr<Shader> waterShader;

    // Instanced meshes (keyed by scene object hash)
    const std::unordered_map<size_t, std::unique_ptr<InstancedMesh>>* instancedMeshes = nullptr;

    // Scene flags (precomputed per frame by app)
    bool hasWaterObjects = false;

    // Post-process
    bool fxaaEnabled = false;
    uint32_t emptyVAO = 0;  // For fullscreen triangle draws
};

} // namespace Puluo
