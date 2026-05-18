#include "puluo/core/EntryPoint.h"
#include "puluo/core/Event.h"
#include "puluo/core/Input.h"
#include "puluo/core/Scene.h"
#include "puluo/core/CommandHistory.h"
#include "puluo/renderer/Renderer.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/renderer/Framebuffer.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/Mesh.h"
#include "puluo/renderer/Camera.h"
#include "puluo/renderer/Light.h"
#include "puluo/renderer/Model.h"
#include "puluo/renderer/IBL.h"
#include "puluo/renderer/Atmosphere.h"
#include "puluo/renderer/Fog.h"
#include "puluo/renderer/Cloud.h"
#include "puluo/renderer/NoiseGenerator.h"
#include "puluo/renderer/SkyLight.h"
#include "puluo/renderer/Buffer.h"
#include "puluo/renderer/VertexArray.h"
#include "puluo/renderer/Terrain.h"
#include "puluo/renderer/Texture2D.h"
#include "puluo/renderer/Frustum.h"
#include "puluo/renderer/CascadedShadowMap.h"
#include "puluo/renderer/SSAO.h"
#include "puluo/renderer/ParticleSystem.h"
#include "puluo/renderer/WeatherSystem.h"
#include "puluo/renderer/InstancedMesh.h"
#include "puluo/renderer/SSR.h"
#include "puluo/renderer/ShadowPass.h"
#include "puluo/renderer/DepthPrepass.h"
#include "puluo/renderer/SSAOPass.h"
#include "puluo/renderer/PostProcessPass.h"
#include "puluo/renderer/RenderContext.h"
#include "puluo/resource/TextureCache.h"
#include "puluo/resource/ModelCache.h"
#include "puluo/resource/AssetImporter.h"
#include "puluo/platform/ImGuiLayer.h"

#include "EditorPanels.h"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <nfd.h>

#include <filesystem>

class SandboxApp : public Puluo::Application {
public:
    SandboxApp()
        : Application({"PuluoEngine Editor", 1920, 1080})
    {
    }

    void OnInit() override {
        Puluo::Renderer::Init();

        // Initialize ImGui
        auto* glfwWindow = static_cast<GLFWwindow*>(GetWindow().GetNativeWindow());
        Puluo::ImGuiLayer::Init(glfwWindow);

        // Initialize native file dialog
        NFD_Init();

        // Scene framebuffer (will be resized to match viewport)
        Puluo::FramebufferSpec fbSpec;
        fbSpec.width = 1920;
        fbSpec.height = 1080;
        fbSpec.entityID = true;
        fbSpec.samples = 4;  // 4x MSAA
        m_SceneFB = std::make_unique<Puluo::Framebuffer>(fbSpec);

        // Post-process framebuffer for FXAA (color only, no entity ID or depth)
        Puluo::FramebufferSpec ppSpec;
        ppSpec.width = 1920;
        ppSpec.height = 1080;
        ppSpec.entityID = false;
        m_PostProcessFB = std::make_unique<Puluo::Framebuffer>(ppSpec);

        // Empty VAO for fullscreen triangle (vertices generated in vertex shader)
        glCreateVertexArrays(1, &m_EmptyVAO);

        // Shaders
        m_PBRShader = Puluo::Shader::CreateFromFile("assets/shaders/pbr.vert",
                                                     "assets/shaders/pbr.frag");
        m_SkyboxShader = Puluo::Shader::CreateFromFile("assets/shaders/skybox.vert",
                                                        "assets/shaders/skybox.frag");
        m_AtmosphereShader = Puluo::Shader::CreateFromFile("assets/shaders/atmosphere.vert",
                                                            "assets/shaders/atmosphere.frag");
        m_CloudShader = Puluo::Shader::CreateFromFile("assets/shaders/cloud.vert",
                                                       "assets/shaders/cloud.frag");
        m_FXAAShader = Puluo::Shader::CreateFromFile("assets/shaders/fxaa.vert",
                                                      "assets/shaders/fxaa.frag");

        // Terrain shader (with tessellation)
        m_TerrainShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/terrain.vert",
            "assets/shaders/terrain.tesc",
            "assets/shaders/terrain.tese",
            "assets/shaders/terrain.frag");

        // Shadow depth shaders
        m_ShadowModelShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/shadow/depth_model.vert",
            "assets/shaders/shadow/depth_model.frag");
        m_ShadowTerrainShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/shadow/depth_terrain.vert",
            "assets/shaders/shadow/depth_terrain.tesc",
            "assets/shaders/shadow/depth_terrain.tese",
            "assets/shaders/shadow/depth_terrain.frag");

        // CSM shadow map
        m_CSM = std::make_unique<Puluo::CascadedShadowMap>();
        m_CSM->Create(m_CSMConfig);

        // Depth pre-pass FBO (depth-only, depth as sampleable texture)
        {
            Puluo::FramebufferSpec depthSpec;
            depthSpec.width = 1920;
            depthSpec.height = 1080;
            depthSpec.depthAsTexture = true;
            depthSpec.colorAttachment = false;
            m_DepthPrepassFB = std::make_unique<Puluo::Framebuffer>(depthSpec);
        }

        // Depth pre-pass shaders
        m_DepthPrepassModelShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/ssao/depth_prepass.vert",
            "assets/shaders/ssao/depth_prepass.frag");
        m_DepthPrepassTerrainShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/ssao/depth_prepass_terrain.vert",
            "assets/shaders/ssao/depth_prepass_terrain.tesc",
            "assets/shaders/ssao/depth_prepass_terrain.tese",
            "assets/shaders/ssao/depth_prepass_terrain.frag");

        // SSAO
        m_SSAO = std::make_unique<Puluo::SSAO>();
        m_SSAO->Create(1920, 1080, m_SSAOConfig);

        // Instanced mesh shaders
        m_PBRInstancedShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/pbr_instanced.vert",
            "assets/shaders/pbr.frag");
        m_ShadowModelInstancedShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/shadow/depth_model_instanced.vert",
            "assets/shaders/shadow/depth_model.frag");
        m_DepthPrepassInstancedShader = Puluo::Shader::CreateFromFile(
            "assets/shaders/ssao/depth_prepass_instanced.vert",
            "assets/shaders/ssao/depth_prepass.frag");

        // SSR
        m_SSR = std::make_unique<Puluo::SSR>();
        m_SSR->Create(1920, 1080);

        // Water shader and mesh
        m_WaterShader = Puluo::Shader::CreateFromFile("assets/shaders/water.vert",
                                                       "assets/shaders/water.frag");
        m_WaterMesh = Puluo::Mesh::CreatePlane(1.0f);

        // Scene color copy texture for water reflection sampling
        {
            glCreateTextures(GL_TEXTURE_2D, 1, &m_SceneColorCopy);
            glBindTexture(GL_TEXTURE_2D, m_SceneColorCopy);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_SceneColorCopyWidth = 1920;
            m_SceneColorCopyHeight = 1080;
        }

        // Generate 3D noise textures for volumetric clouds
        {
            auto baseData = Puluo::NoiseGenerator::GenerateBaseNoise(128);
            glGenTextures(1, &m_BaseNoiseTexture);
            glBindTexture(GL_TEXTURE_3D, m_BaseNoiseTexture);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 128, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, baseData.data());
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            auto detailData = Puluo::NoiseGenerator::GenerateDetailNoise(32);
            glGenTextures(1, &m_DetailNoiseTexture);
            glBindTexture(GL_TEXTURE_3D, m_DetailNoiseTexture);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 32, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, detailData.data());
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        // Default terrain scene object
        {
            Puluo::SceneTerrainData td;
            td.worldSize = 500.0f;
            td.heightmapRes = 257;
            td.patchCount = 64;
            td.heightScale = 80.0f;
            td.uvScale = 50.0f;
            td.noiseFreq = 3.0f;
            td.noiseOctaves = 6;
            td.created = true;

            m_TerrainParams.worldSize = td.worldSize;
            m_TerrainParams.heightmapRes = td.heightmapRes;
            m_TerrainParams.patchCount = td.patchCount;
            m_TerrainParams.heightScale = td.heightScale;
            m_TerrainParams.uvScale = td.uvScale;
            m_Terrain.Create(m_TerrainParams);
            m_Terrain.GenerateFromNoise(td.noiseFreq, td.noiseOctaves);

            auto& terrainObj = m_Scene.AddObject("Terrain", nullptr);
            terrainObj.terrain = td;
        }

        // Camera
        m_Camera = Puluo::CameraController(45.0f, 1920.0f / 1080.0f);

        // Lights as scene objects
        m_Lights.SetAmbient({0.03f, 0.03f, 0.03f});

        {
            auto& dirObj = m_Scene.AddObject("Directional Light", nullptr);
            Puluo::SceneLightData dirLight;
            dirLight.type = 0; // Directional
            dirLight.color = {1.0f, 0.95f, 0.9f};
            dirLight.intensity = 2.0f;
            dirObj.light = dirLight;
            // Direction is derived from transform rotation; set a default rotation
            dirObj.transform.SetEulerDegrees({-60.0f, -30.0f, 0.0f});
        }

        {
            auto& pointObj = m_Scene.AddObject("Point Light", nullptr);
            Puluo::SceneLightData pointLight;
            pointLight.type = 1; // Point
            pointLight.color = {0.3f, 0.5f, 1.0f};
            pointLight.intensity = 5.0f;
            pointObj.light = pointLight;
            pointObj.transform.position = {2.0f, 2.0f, 2.0f};
        }

        // IBL - load HDR environment map
        Puluo::IBLPreprocessor iblProcessor;
        m_IBLMaps = iblProcessor.Process("assets/textures/hdr/environment.hdr");
        if (m_IBLMaps.irradianceMap) {
            Puluo::Renderer::SetIBLMaps(&m_IBLMaps);
            PULUO_INFO("IBL maps loaded successfully.");
        } else {
            PULUO_CORE_WARN("Failed to load HDR environment map. IBL disabled.");
        }

        // Load default model into scene

        // Sky light for atmosphere IBL
        m_SkyLight = std::make_unique<Puluo::SkyLight>();
        auto defaultModel = Puluo::ModelCache::Load("assets/models/model.glb");
        if (defaultModel) {
            auto& obj = m_Scene.AddObject("Default Model", defaultModel);
            obj.modelPath = "assets/models/model.glb";
            obj.transform.position = {0.0f, 0.0f, 0.0f};
            m_Scene.Select(0);
        }

        PULUO_INFO("Editor initialized! Right-click + WASD to move camera.");

        // Particle system
        m_ParticleSystem.Init();

        // Weather system
        m_WeatherSystem.Init();

        // Line shader for grid and axes
        m_LineShader = Puluo::Shader::CreateFromFile("assets/shaders/line.vert",
                                                      "assets/shaders/line.frag");
         
        // Build grid VAO (XZ plane, -100 to +100, spacing 1, every 10 lines brighter)
        {
            const int halfSize = 100;
            std::vector<float> vertices;
            for (int i = -halfSize; i <= halfSize; i++) {
                float fi = static_cast<float>(i);
                float gray;
                if (i == 0) gray = 0.55f;
                else if (i % 10 == 0) gray = 0.45f;
                else gray = 0.25f;
                // Line along X axis (varying Z)
                vertices.insert(vertices.end(), {static_cast<float>(-halfSize), 0.0f, fi, gray, gray, gray});
                vertices.insert(vertices.end(), {static_cast<float>(halfSize), 0.0f, fi, gray, gray, gray});
                // Line along Z axis (varying X)
                vertices.insert(vertices.end(), {fi, 0.0f, static_cast<float>(-halfSize), gray, gray, gray});
                vertices.insert(vertices.end(), {fi, 0.0f, static_cast<float>(halfSize), gray, gray, gray});
            }
            m_GridVertexCount = static_cast<uint32_t>(vertices.size() / 6);

            auto vbo = std::make_shared<Puluo::VertexBuffer>(vertices.data(),
                static_cast<uint32_t>(vertices.size() * sizeof(float)));
            vbo->SetLayout({
                {"aPosition", Puluo::ShaderDataType::Float3},
                {"aColor",    Puluo::ShaderDataType::Float3}
            });
            m_GridVAO = std::make_shared<Puluo::VertexArray>();
            m_GridVAO->AddVertexBuffer(vbo);
        }

        // Build axis VAO (RGB axes from origin, length 1.5)
        {
            float axisLen = 1.5f;
            float axisVerts[] = {
                // X axis - red
                0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
                axisLen, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
                // Y axis - green
                0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
                0.0f, axisLen, 0.0f,  0.0f, 1.0f, 0.0f,
                // Z axis - blue
                0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, axisLen,  0.0f, 0.0f, 1.0f,
            };
            m_AxisVertexCount = 6;

            auto vbo = std::make_shared<Puluo::VertexBuffer>(axisVerts, sizeof(axisVerts));
            vbo->SetLayout({
                {"aPosition", Puluo::ShaderDataType::Float3},
                {"aColor",    Puluo::ShaderDataType::Float3}
            });
            m_AxisVAO = std::make_shared<Puluo::VertexArray>();
            m_AxisVAO->AddVertexBuffer(vbo);
        }
    }

    void OnUpdate(float dt) override {
        m_DeltaTime = dt;

        // Cloud time accumulation
        m_CloudTime += dt;

        // Particle emission and simulation
        m_ParticleSystem.BeginFrame();
        auto& objects = m_Scene.GetObjects();
        for (size_t i = 0; i < objects.size(); i++) {
            auto& obj = objects[i];
            if (obj.particle.has_value() && obj.particle->enabled) {
                auto& pd = obj.particle.value();
                Puluo::ParticleEmitterParams params;
                params.enabled = pd.enabled;
                params.emissionRate = pd.emissionRate;
                params.maxParticles = pd.maxParticles;
                params.velocityMin = pd.velocityMin;
                params.velocityMax = pd.velocityMax;
                params.lifetimeMin = pd.lifetimeMin;
                params.lifetimeMax = pd.lifetimeMax;
                params.sizeStart = pd.sizeStart;
                params.sizeEnd = pd.sizeEnd;
                params.colorStart = pd.colorStart;
                params.colorEnd = pd.colorEnd;
                params.gravity = pd.gravity;
                params.drag = pd.drag;
                params.blendMode = static_cast<Puluo::ParticleBlendMode>(pd.blendMode);
                m_ParticleSystem.EmitFrom(static_cast<uint32_t>(i),
                                          obj.transform.position, params, dt);
            }
        }
        m_ParticleSystem.Update(dt);
        m_WeatherSystem.Update(dt, m_Camera.GetPosition());

        // Camera: enabled only when hovering viewport AND not manipulating gizmo
        // Don't use WantCaptureMouse — docking makes it always true
        bool canControl = m_ViewportHovered && !ImGuizmo::IsUsing();
        m_Camera.SetInputEnabled(canControl);

        m_Camera.OnUpdate(dt);

        // Gizmo mode shortcuts: only when viewport focused, no text input, no right-click
        if ((m_ViewportFocused || m_ViewportHovered)
            && !ImGui::GetIO().WantTextInput
            && !Puluo::Input::IsMouseButtonPressed(Puluo::MouseButton::Right)) {
            if (Puluo::Input::IsKeyPressed(Puluo::Key::W)) m_GizmoMode = Puluo::GizmoMode::Translate;
            if (Puluo::Input::IsKeyPressed(Puluo::Key::E)) m_GizmoMode = Puluo::GizmoMode::Rotate;
            if (Puluo::Input::IsKeyPressed(Puluo::Key::R)) m_GizmoMode = Puluo::GizmoMode::Scale;
        }

        // Toggle stats overlay
        if (Puluo::Input::IsKeyPressed(Puluo::Key::F3))
            m_ShowStatsOverlay = !m_ShowStatsOverlay;

        if (Puluo::Input::IsKeyPressed(Puluo::Key::Escape))
            Close();
    }

    void OnRender() override {
        // Skip rendering when window is minimized (framebuffer size 0x0)
        int winW = GetWindow().GetWidth();
        int winH = GetWindow().GetHeight();
        if (winW <= 0 || winH <= 0) return;

        // (Atmosphere sun is injected as directional light during LightManager rebuild below)

        // ---- Per-instance frustum + distance culling (shared by all passes) ----
        Puluo::Frustum frustum = Puluo::Frustum::FromVPMatrix(m_Camera.GetViewProjection());
        Puluo::Renderer::ResetFrameStats();
        if (!m_InstancedMeshes.empty()) {
            for (auto& [idx, im] : m_InstancedMeshes) {
                im->CullAndUpload(frustum, m_Camera.GetPosition(), m_InstancedMaxDrawDistance);
            }
        }

        // ---- CSM Shadow Pass ----
        {
            // Compute shadow light direction
            Puluo::Vec3 shadowLightDir(0.0f, -1.0f, 0.0f);
            if (m_UseAtmosphere) {
                shadowLightDir = m_AtmosphereParams.sunDirection;
            } else {
                for (auto& obj : m_Scene.GetObjects()) {
                    if (obj.light.has_value() && obj.light->type == 0) {
                        shadowLightDir = -glm::normalize(obj.transform.orientation * Puluo::Vec3(0.0f, 0.0f, -1.0f));
                        break;
                    }
                }
            }

            // Populate render context for shadow pass
            m_RenderCtx.camera = &m_Camera;
            m_RenderCtx.scene = &m_Scene;
            m_RenderCtx.csm = m_CSM.get();
            m_RenderCtx.csmConfig = &m_CSMConfig;
            m_RenderCtx.shadowLightDir = shadowLightDir;
            m_RenderCtx.shadowModelShader = m_ShadowModelShader;
            m_RenderCtx.shadowModelInstancedShader = m_ShadowModelInstancedShader;
            m_RenderCtx.shadowTerrainShader = m_ShadowTerrainShader;
            m_RenderCtx.terrain = &m_Terrain;
            m_RenderCtx.terrainParams = &m_TerrainParams;
            m_RenderCtx.instancedMeshes = &m_InstancedMeshes;

            m_ShadowPass.Run(m_RenderCtx);
        }

        // ---- Depth Pre-pass (for SSAO / SSR / Water shore fade) ----
        {
            bool hasWaterObjects = false;
            for (auto& obj : m_Scene.GetObjects()) {
                if (obj.water.has_value()) { hasWaterObjects = true; break; }
            }
            m_RenderCtx.hasWaterObjects = hasWaterObjects;
            m_RenderCtx.ssao = m_SSAO.get();
            m_RenderCtx.ssaoConfig = &m_SSAOConfig;
            m_RenderCtx.ssrConfig = &m_SSRConfig;
            m_RenderCtx.depthPrepassFB = m_DepthPrepassFB.get();
            m_RenderCtx.depthPrepassModelShader = m_DepthPrepassModelShader;
            m_RenderCtx.depthPrepassInstancedShader = m_DepthPrepassInstancedShader;
            m_RenderCtx.depthPrepassTerrainShader = m_DepthPrepassTerrainShader;

            m_DepthPrepassPass.Run(m_RenderCtx);

            // ---- SSAO Generate + Blur ----
            m_RenderCtx.emptyVAO = m_EmptyVAO;
            m_SSAOPass.Run(m_RenderCtx);
        }

        // ---- Render scene to FBO ----
        m_SceneFB->Bind();
        Puluo::RenderCommand::SetClearColor({0.12f, 0.12f, 0.15f, 1.0f});
        Puluo::RenderCommand::Clear();
        m_SceneFB->ClearEntityIDAttachment(-1);

        // Rebuild LightManager from scene light objects
        m_Lights.Clear();
        float ambientVal = m_UseAtmosphere
            ? glm::clamp(m_AtmosphereParams.sunDirection.y * 0.3f + 0.05f, 0.02f, 0.15f)
            : 0.03f;
        m_Lights.SetAmbient(Puluo::Vec3(ambientVal));
        // Compute sun brightness factor for atmosphere mode
        float sunBrightness = m_UseAtmosphere ? m_AtmosphereParams.ComputeSunBrightness() : 1.0f;
        // Compute sunset color tint via Rayleigh extinction (warm at sunset, white at noon)
        Puluo::Vec3 sunTint(1.0f);
        if (m_UseAtmosphere) {
            float sunY = glm::clamp(m_AtmosphereParams.sunDirection.y, 0.0f, 1.0f);
            float airMass = glm::min(1.0f / glm::max(sunY + 0.01f, 0.01f), 40.0f);
            const Puluo::Vec3 BETA_R{5.8e-6f, 13.5e-6f, 33.1e-6f};
            float scale = 8000.0f * airMass;
            sunTint = Puluo::Vec3(std::exp(-BETA_R.x * scale),
                                   std::exp(-BETA_R.y * scale),
                                   std::exp(-BETA_R.z * scale));
            float maxT = glm::max(glm::max(sunTint.x, sunTint.y), sunTint.z);
            if (maxT > 1e-6f) sunTint /= maxT;

            // Inject atmosphere sun as the primary directional light
            Puluo::Light sunLight;
            sunLight.type = Puluo::LightType::Directional;
            sunLight.direction = -m_AtmosphereParams.sunDirection;
            sunLight.color = sunTint;
            sunLight.intensity = m_AtmosphereParams.sunIntensity * sunBrightness;
            m_Lights.AddLight(sunLight);
        }
        for (auto& obj : m_Scene.GetObjects()) {
            if (!obj.light.has_value()) continue;
            auto& ld = obj.light.value();
            // Skip scene directional lights when atmosphere provides the sun
            if (m_UseAtmosphere && ld.type == 0) continue;
            Puluo::Light light;
            light.type = static_cast<Puluo::LightType>(ld.type);
            light.color = ld.color;
            light.intensity = ld.intensity;
            light.position = obj.transform.position;
            light.direction = glm::normalize(obj.transform.orientation * Puluo::Vec3(0.0f, 0.0f, -1.0f));
            light.constant = ld.constant;
            light.linear = ld.linear;
            light.quadratic = ld.quadratic;
            light.innerCutoff = glm::cos(glm::radians(ld.innerCutoffDeg));
            light.outerCutoff = glm::cos(glm::radians(ld.outerCutoffDeg));
            m_Lights.AddLight(light);
        }

        Puluo::Renderer::BeginScene(m_Camera, m_Lights);

        // Auto-derive fog colors from atmosphere
        if (m_FogParams.enabled && m_UseAtmosphere) {
            m_FogParams.directionalInscatteringColor = m_AtmosphereParams.ComputeSunTransmittanceColor();
            m_FogParams.sunDirection = m_AtmosphereParams.sunDirection;
            // Dim base fog color at night (base blue × sun brightness)
            float brightness = m_AtmosphereParams.ComputeSunBrightness();
            m_FogParams.fogColor = Puluo::Vec3(0.18f, 0.28f, 0.48f) * brightness;
        }
        Puluo::Renderer::SetFogParams(m_FogParams.enabled ? &m_FogParams : nullptr);

        // Set IBL maps before model rendering based on sky mode
        if (m_UseAtmosphere) {
            m_SkyLight->Update(m_AtmosphereParams);
            if (m_SkyLight->IsValid()) {
                Puluo::Renderer::SetIBLMaps(&m_SkyLight->GetIBLMaps());
            } else {
                Puluo::Renderer::SetIBLMaps(nullptr);
            }
        } else {
            Puluo::Renderer::SetIBLMaps(&m_IBLMaps);
        }

        // Set CSM shadow map for PBR/terrain rendering
        Puluo::Renderer::SetShadowMap(
            (m_CSM && m_CSM->IsCreated()) ? m_CSM.get() : nullptr);

        // Set SSAO map for PBR/terrain rendering
        {
            auto& fbSpec = m_SceneFB->GetSpec();
            Puluo::Vec2 fbSize(static_cast<float>(fbSpec.width), static_cast<float>(fbSpec.height));
            Puluo::Renderer::SetSSAOMap(
                (m_SSAO && m_SSAOConfig.enabled) ? m_SSAO.get() : nullptr, fbSize);
        }

        // Draw grid (before models, will be occluded naturally)
        m_LineShader->Bind();
        m_LineShader->SetMat4("uViewProjection", m_Camera.GetViewProjection());
        m_LineShader->SetVec3("uCamPos", m_Camera.GetPosition());
        if (m_FogParams.enabled) {
            m_LineShader->SetInt("uFogEnabled", 1);
            m_LineShader->SetFloat("uFogDensity", m_FogParams.density);
            m_LineShader->SetFloat("uFogHeightFalloff", m_FogParams.heightFalloff);
            m_LineShader->SetFloat("uFogMaxOpacity", m_FogParams.maxOpacity);
            m_LineShader->SetVec3("uFogColor", m_FogParams.fogColor);
            m_LineShader->SetFloat("uFogStartDistance", m_FogParams.startDistance);
            m_LineShader->SetVec3("uFogDirInscatterColor", m_FogParams.directionalInscatteringColor);
            m_LineShader->SetFloat("uFogDirInscatterExp", m_FogParams.directionalInscatteringExponent);
            m_LineShader->SetFloat("uFogDirInscatterStartDist", m_FogParams.directionalInscatteringStartDistance);
            m_LineShader->SetVec3("uFogSunDirection", m_FogParams.sunDirection);
        } else {
            m_LineShader->SetInt("uFogEnabled", 0);
        }
        Puluo::RenderCommand::DrawLines(m_GridVAO, m_GridVertexCount);

        // Render terrain
        if (m_Terrain.IsCreated() && m_TerrainShader) {
            Puluo::Vec3 terrainPos(0.0f);
            for (auto& obj : m_Scene.GetObjects()) {
                if (obj.terrain.has_value()) { terrainPos = obj.transform.position; break; }
            }
            Puluo::Renderer::RenderTerrain(m_TerrainShader, m_Camera, m_Terrain, m_TerrainParams, m_TerrainMaterial, m_AtmosphereParams, terrainPos);
        }

        // Frustum culling for models (frustum already computed above)

        auto& objects = m_Scene.GetObjects();
        for (size_t i = 0; i < objects.size(); i++) {
            auto& obj = objects[i];
            if (obj.model && !obj.model->GetMeshes().empty()) {
                // Frustum cull: transform model AABB to world space and test
                Puluo::Mat4 modelMatrix = obj.transform.ToMatrix();
                Puluo::AABB worldAABB = Puluo::TransformAABB(obj.model->GetBoundingBox(), modelMatrix);
                if (!frustum.TestAABB(worldAABB)) {
                    Puluo::Renderer::IncrementCulled();
                    continue;
                }
                Puluo::Renderer::SubmitModel(m_PBRShader, *obj.model, modelMatrix, static_cast<int>(i));
                Puluo::Renderer::IncrementDrawCall();
            }
        }

        // Render instanced meshes (PBR, culled)
        if (m_PBRInstancedShader && !m_InstancedMeshes.empty()) {
            m_PBRInstancedShader->Bind();
            m_PBRInstancedShader->SetMat4("uViewProjection", m_Camera.GetViewProjection());
            Puluo::Renderer::BindFrameUniforms(m_PBRInstancedShader);
            for (auto& [idx, im] : m_InstancedMeshes) {
                if (im->GetVisibleCount() == 0) continue;
                m_PBRInstancedShader->SetInt("uEntityID", static_cast<int>(idx));
                im->DrawWithMaterialsCulled(m_PBRInstancedShader);
            }
        }

        if (m_UseAtmosphere) {
            Puluo::Renderer::RenderAtmosphere(m_AtmosphereShader, m_Camera, m_AtmosphereParams);
        } else {
            Puluo::Renderer::RenderSkybox(m_SkyboxShader, m_Camera);
        }

        // Volumetric clouds (rendered after sky, with alpha blending)
        if (m_CloudParams.enabled && m_UseAtmosphere) {
            Puluo::Renderer::RenderClouds(m_CloudShader, m_Camera, m_CloudParams,
                                           m_AtmosphereParams, m_BaseNoiseTexture,
                                           m_DetailNoiseTexture, m_CloudTime);
        }

        // Render particles (after sky/clouds, before water)
        m_ParticleSystem.Render(m_Camera);

        // Render weather particles (rain/snow)
        m_WeatherSystem.Render(m_Camera, m_WeatherConfig);

        // ---- Water Rendering ----
        {
            bool hasWater = false;
            for (auto& obj : m_Scene.GetObjects()) {
                if (obj.water.has_value()) { hasWater = true; break; }
            }
            if (hasWater && m_WaterShader) {
                // Resolve MSAA before copying scene color
                m_SceneFB->Resolve();

                // Copy current scene color to avoid read-write hazard
                auto& fbSpec = m_SceneFB->GetSpec();
                if (m_SceneColorCopyWidth != fbSpec.width || m_SceneColorCopyHeight != fbSpec.height) {
                    glDeleteTextures(1, &m_SceneColorCopy);
                    glCreateTextures(GL_TEXTURE_2D, 1, &m_SceneColorCopy);
                    glBindTexture(GL_TEXTURE_2D, m_SceneColorCopy);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbSpec.width, fbSpec.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    m_SceneColorCopyWidth = fbSpec.width;
                    m_SceneColorCopyHeight = fbSpec.height;
                }
                glCopyImageSubData(
                    m_SceneFB->GetColorAttachmentID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                    m_SceneColorCopy, GL_TEXTURE_2D, 0, 0, 0, 0,
                    fbSpec.width, fbSpec.height, 1);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                m_WaterShader->Bind();
                m_WaterShader->SetMat4("uViewProjection", m_Camera.GetViewProjection());
                m_WaterShader->SetVec3("uCamPos", m_Camera.GetPosition());
                m_WaterShader->SetFloat("uTime", m_CloudTime);
                m_WaterShader->SetVec2("uScreenSize", Puluo::Vec2(
                    static_cast<float>(fbSpec.width), static_cast<float>(fbSpec.height)));
                m_WaterShader->SetMat4("uProjection", m_Camera.GetProjectionMatrix());
                m_WaterShader->SetInt("uSceneColor", 10);
                glBindTextureUnit(10, m_SceneColorCopy);

                if (m_DepthPrepassFB) {
                    m_WaterShader->SetInt("uDepthTexture", 11);
                    glBindTextureUnit(11, m_DepthPrepassFB->GetDepthAttachmentID());
                }

                auto& sceneObjects = m_Scene.GetObjects();
                for (size_t i = 0; i < sceneObjects.size(); i++) {
                    auto& obj = sceneObjects[i];
                    if (!obj.water.has_value()) continue;
                    auto& wd = obj.water.value();

                    Puluo::Mat4 model = obj.transform.ToMatrix();
                    model = glm::scale(model, Puluo::Vec3(wd.planeSize));

                    m_WaterShader->SetMat4("uModel", model);
                    m_WaterShader->SetVec3("uWaterTint", wd.tintColor);
                    m_WaterShader->SetFloat("uWaveSpeed", wd.waveSpeed);
                    m_WaterShader->SetFloat("uWaveScale", wd.waveScale);
                    m_WaterShader->SetFloat("uReflectionStrength", wd.reflectionStrength);
                    m_WaterShader->SetFloat("uOpacity", wd.opacity);
                    m_WaterShader->SetFloat("uFresnelPower", wd.fresnelPower);
                    m_WaterShader->SetInt("uEntityID", static_cast<int>(i));

                    m_WaterMesh.Draw();
                }

                glDisable(GL_BLEND);
            }
        }

        // Draw coordinate axes (always visible, on top of everything)
        Puluo::RenderCommand::SetDepthTest(false);
        m_LineShader->Bind();
        m_LineShader->SetMat4("uViewProjection", m_Camera.GetViewProjection());
        Puluo::RenderCommand::SetLineWidth(2.5f);
        Puluo::RenderCommand::DrawLines(m_AxisVAO, m_AxisVertexCount);
        Puluo::RenderCommand::SetLineWidth(1.0f);
        Puluo::RenderCommand::SetDepthTest(true);

        Puluo::Renderer::EndScene();

        m_SceneFB->Resolve();  // Resolve MSAA before post-process reads the texture
        m_SceneFB->Unbind();

        // ---- Post-process (SSR + FXAA) ----
        {
            m_RenderCtx.sceneFB = m_SceneFB.get();
            m_RenderCtx.postProcessFB = m_PostProcessFB.get();
            m_RenderCtx.ssr = m_SSR.get();
            m_RenderCtx.ssrConfig = &m_SSRConfig;
            m_RenderCtx.fxaaShader = m_FXAAShader;
            m_RenderCtx.fxaaEnabled = m_FXAAEnabled;
            m_RenderCtx.saturation = m_Saturation;
            m_RenderCtx.contrast = m_Contrast;

            m_PostProcessPass.Run(m_RenderCtx);
        }

        // ---- Clear default framebuffer ----
        Puluo::RenderCommand::SetClearColor({0.1f, 0.1f, 0.1f, 1.0f});
        Puluo::RenderCommand::Clear();

        // ---- ImGui Editor UI ----
        Puluo::ImGuiLayer::BeginFrame();
        ImGuizmo::BeginFrame();

        // Fullscreen dockspace
        ImGuiWindowFlags dockspaceFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
            | ImGuiWindowFlags_NoBackground;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, dockspaceFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // Default layout on first run
        if (m_FirstFrame) {
            m_FirstFrame = false;

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

            ImGuiID dockLeft, dockCenterRight;
            ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.18f, &dockLeft, &dockCenterRight);

            ImGuiID dockCenter, dockRight;
            ImGui::DockBuilderSplitNode(dockCenterRight, ImGuiDir_Right, 0.22f, &dockRight, &dockCenter);

            ImGuiID dockRightTop, dockRightBottom;
            ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.30f, &dockRightBottom, &dockRightTop);

            // Split bottom from center for Asset Browser
            ImGuiID dockCenterTop, dockBottom;
            ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.25f, &dockBottom, &dockCenterTop);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
            ImGui::DockBuilderDockWindow("Viewport", dockCenterTop);
            ImGui::DockBuilderDockWindow("Inspector", dockRightTop);
            ImGui::DockBuilderDockWindow("Toolbar", dockRightBottom);
            ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        // Menu bar
        bool wantsImport = false;
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveScene();
                if (ImGui::MenuItem("Save Scene As...")) SaveSceneAs();
                if (ImGui::MenuItem("Load Scene...", "Ctrl+O")) LoadScene();
                ImGui::Separator();
                if (ImGui::MenuItem("Import Model...", "Ctrl+I")) {
                    wantsImport = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    Close();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_CommandHistory.CanUndo())) m_CommandHistory.Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_CommandHistory.CanRedo())) m_CommandHistory.Redo();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("Theme")) {
                    auto current = Puluo::ImGuiLayer::GetCurrentTheme();
                    for (int i = 0; i < static_cast<int>(Puluo::EditorTheme::Count); ++i) {
                        auto t = static_cast<Puluo::EditorTheme>(i);
                        bool selected = (t == current);
                        if (ImGui::MenuItem(Puluo::ImGuiLayer::GetThemeName(t), nullptr, selected)) {
                            Puluo::ImGuiLayer::ApplyTheme(t);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                ImGui::MenuItem("Stats Overlay", "F3", &m_ShowStatsOverlay);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End(); // DockSpace

        // ---- Viewport window: show FBO texture ----
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");

        ImVec2 vpSize = ImGui::GetContentRegionAvail();
        uint32_t vpW = static_cast<uint32_t>(vpSize.x);
        uint32_t vpH = static_cast<uint32_t>(vpSize.y);

        // Resize FBO if viewport size changed
        if (vpW > 0 && vpH > 0) {
            auto& fbSpec = m_SceneFB->GetSpec();
            if (fbSpec.width != vpW || fbSpec.height != vpH) {
                m_SceneFB->Resize(vpW, vpH);
                m_PostProcessFB->Resize(vpW, vpH);
                if (m_DepthPrepassFB) m_DepthPrepassFB->Resize(vpW, vpH);
                if (m_SSAO) m_SSAO->Resize(vpW, vpH);
                if (m_SSR) m_SSR->Resize(vpW, vpH);
                m_Camera.SetAspectRatio(static_cast<float>(vpW) / static_cast<float>(vpH));
            }
        }

        // Display FBO texture (flipped UV for OpenGL)
        uint32_t texID = m_PostProcessPass.IsEnabled()
            ? m_PostProcessFB->GetColorAttachmentID()
            : m_SceneFB->GetColorAttachmentID();
        ImVec2 vpMin = ImGui::GetCursorScreenPos();
        ImGui::SetNextItemAllowOverlap();
        ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(texID)),
                     vpSize, ImVec2(0, 1), ImVec2(1, 0));
        m_ViewportHovered = ImGui::IsItemHovered();
        m_ViewportFocused = ImGui::IsWindowFocused();

        // Mouse picking / splat brush: left-click in viewport
        auto& splatBrush = Puluo::GetSplatBrushState();
        bool splatPainting = splatBrush.enabled && m_Terrain.IsCreated() && m_Terrain.HasSplatMap();

        if (m_ViewportHovered
            && (splatPainting ? ImGui::IsMouseDown(ImGuiMouseButton_Left) : ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()
            && !Puluo::Input::IsMouseButtonPressed(Puluo::MouseButton::Right)) {

            if (splatPainting) {
                // Splat brush: raycast against terrain and paint
                ImVec2 mousePos = ImGui::GetMousePos();
                float mx = (mousePos.x - vpMin.x) / vpSize.x * 2.0f - 1.0f;
                float my = 1.0f - (mousePos.y - vpMin.y) / vpSize.y * 2.0f;

                Puluo::Mat4 invVP = glm::inverse(m_Camera.GetViewProjection());
                Puluo::Vec4 nearClip = invVP * Puluo::Vec4(mx, my, -1.0f, 1.0f);
                Puluo::Vec4 farClip = invVP * Puluo::Vec4(mx, my, 1.0f, 1.0f);
                nearClip /= nearClip.w;
                farClip /= farClip.w;

                Puluo::Vec3 rayOrigin(nearClip);
                Puluo::Vec3 rayDir = glm::normalize(Puluo::Vec3(farClip) - rayOrigin);

                auto hit = m_Terrain.Raycast(rayOrigin, rayDir);
                if (hit.hit) {
                    m_Terrain.PaintSplat(hit.position.x, hit.position.z,
                        splatBrush.layer, splatBrush.radius, splatBrush.strength, splatBrush.eraseMode);
                }
            } else {
                // Normal entity picking
                ImVec2 mousePos = ImGui::GetMousePos();
                int pixX = static_cast<int>(mousePos.x - vpMin.x);
                int pixY = static_cast<int>(vpSize.y - (mousePos.y - vpMin.y));
                auto& fbSpec = m_SceneFB->GetSpec();
                if (pixX >= 0 && pixY >= 0 && pixX < static_cast<int>(fbSpec.width) && pixY < static_cast<int>(fbSpec.height)) {
                    int entityID = m_SceneFB->ReadPixel(1, pixX, pixY);
                    if (entityID >= 0 && entityID < static_cast<int>(m_Scene.GetObjects().size())) {
                        m_Scene.Select(static_cast<size_t>(entityID));
                    } else {
                        m_Scene.ClearSelection();
                    }
                }
            }
        }

        // Gizmo overlay (uses ImGuizmo's own input handling, works over ImGui::Image)
        if (auto* selected = m_Scene.GetSelected()) {
            // Snapshot transform when gizmo starts being used
            bool isUsing = ImGuizmo::IsUsing();
            if (isUsing && !m_GizmoWasUsing) {
                m_GizmoStartTransform = selected->transform;
            }

            ImVec2 vpMax = ImVec2(vpMin.x + vpSize.x, vpMin.y + vpSize.y);
            Puluo::DrawGizmo(*selected,
                             m_Camera.GetViewMatrix(),
                             m_Camera.GetProjectionMatrix(),
                             m_GizmoMode,
                             vpMin.x, vpMin.y,
                             vpMax.x - vpMin.x, vpMax.y - vpMin.y);

            // Record undo command when gizmo stops being used
            if (!ImGuizmo::IsUsing() && m_GizmoWasUsing) {
                int selIdx = m_Scene.GetSelectedIndex();
                if (selIdx >= 0) {
                    auto cmd = std::make_unique<Puluo::TransformChangeCommand>(
                        m_Scene, static_cast<size_t>(selIdx),
                        m_GizmoStartTransform, selected->transform);
                    m_CommandHistory.PushExecuted(std::move(cmd));
                }
            }
            m_GizmoWasUsing = isUsing;
        }

        // Stats overlay pinned to Viewport top-right (F3 to toggle)
        Puluo::DrawStatsOverlay(&m_ShowStatsOverlay, vpMin.x, vpMin.y, vpSize.x, vpSize.y);

        ImGui::End(); // Viewport
        ImGui::PopStyleVar();

        // Editor panels
        bool toolbarImport = false;
        Puluo::DrawToolbar(m_GizmoMode, toolbarImport, m_Camera, m_UseAtmosphere, m_AtmosphereParams, m_FogParams, m_CloudParams, m_FXAAEnabled, m_Saturation, m_Contrast, m_SSAOConfig, m_SSRConfig, m_WeatherConfig);
        wantsImport = wantsImport || toolbarImport;

        Puluo::DrawSceneHierarchy(m_Scene, m_CommandHistory);
        Puluo::DrawInspector(m_Scene, m_CommandHistory, m_Terrain, m_TerrainParams, m_TerrainMaterial, m_InstancedMeshes, m_InstancedMaxDrawDistance);

        // Check if instanced meshes need rebuilding (triggered by editor)
        if (Puluo::ConsumeInstancedMeshRebuildFlag()) {
            // Rebuild map: clear and re-add all ISM objects
            m_InstancedMeshes.clear();
            auto& objs = m_Scene.GetObjects();
            for (size_t i = 0; i < objs.size(); i++) {
                if (objs[i].instancedMesh.has_value()) {
                    RebuildInstancedMesh(i);
                }
            }
        }

        // Handle terrain create/regenerate signals from inspector
        if (Puluo::ConsumeTerrainCreateFlag()) {
            // Find terrain scene object and create runtime terrain
            for (auto& obj : m_Scene.GetObjects()) {
                if (obj.terrain.has_value()) {
                    auto& td = obj.terrain.value();
                    m_TerrainParams.worldSize = td.worldSize;
                    m_TerrainParams.heightmapRes = td.heightmapRes;
                    m_TerrainParams.patchCount = td.patchCount;
                    m_TerrainParams.heightScale = td.heightScale;
                    m_TerrainParams.uvScale = td.uvScale;
                    m_Terrain.Create(m_TerrainParams);
                    m_Terrain.GenerateFromNoise(td.noiseFreq, td.noiseOctaves);
                    td.created = true;
                    break;
                }
            }
        }
        if (Puluo::ConsumeTerrainRegenerateFlag()) {
            for (auto& obj : m_Scene.GetObjects()) {
                if (obj.terrain.has_value() && m_Terrain.IsCreated()) {
                    auto& td = obj.terrain.value();
                    m_Terrain.GenerateFromNoise(td.noiseFreq, td.noiseOctaves);
                    break;
                }
            }
        }

        // Handle heightmap image load
        {
            std::string hmPath = Puluo::ConsumeTerrainHeightmapLoadPath();
            if (!hmPath.empty() && m_Terrain.IsCreated()) {
                m_Terrain.LoadFromImage(hmPath);
            }
        }

        // Handle splat map generate from rules
        if (Puluo::ConsumeSplatGenerateFlag() && m_Terrain.IsCreated()) {
            m_Terrain.GenerateSplatFromRules(
                m_TerrainMaterial.heightThreshold,
                m_TerrainMaterial.slopeThreshold,
                m_TerrainMaterial.blendSharpness);
        }

        // If terrain scene object was deleted, destroy runtime terrain
        if (m_Terrain.IsCreated()) {
            bool hasTerrainObj = false;
            for (auto& obj : m_Scene.GetObjects()) {
                if (obj.terrain.has_value()) { hasTerrainObj = true; break; }
            }
            if (!hasTerrainObj) {
                m_Terrain.Destroy();
                m_TerrainMaterial = Puluo::TerrainMaterial{};
                m_LoadedTerrainTextures.clear();
            }
        }

        // Asset Browser
        std::string browserImportPath;
        std::string browserScenePath;
        Puluo::DrawAssetBrowser(browserImportPath, browserScenePath);

        Puluo::ImGuiLayer::EndFrame();

        // Handle model import (after ImGui frame to avoid ID conflicts)
        if (wantsImport) {
            ImportModel();
        }
        if (!browserImportPath.empty()) {
            // If it's a raw model from browser, import to project first
            std::string ext = std::filesystem::path(browserImportPath).extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext != ".passet") {
                std::string passetPath = Puluo::AssetImporter::ImportModelToProject(browserImportPath);
                if (!passetPath.empty()) browserImportPath = passetPath;
            }
            ImportModelFromPath(browserImportPath);
        }
        if (!browserScenePath.empty()) {
            LoadSceneFromPath(browserScenePath);
        }
    }

    void OnEvent(Puluo::Event& event) override {
        Puluo::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Puluo::WindowResizeEvent>([this](Puluo::WindowResizeEvent& e) {
            if (e.GetWidth() > 0 && e.GetHeight() > 0)
                Puluo::RenderCommand::SetViewport(0, 0, e.GetWidth(), e.GetHeight());
            return false;
        });
        dispatcher.Dispatch<Puluo::KeyPressedEvent>([this](Puluo::KeyPressedEvent& e) {
            if (e.IsRepeat() || ImGui::GetIO().WantTextInput) return false;
            bool ctrl = Puluo::Input::IsKeyPressed(Puluo::Key::LeftControl)
                     || Puluo::Input::IsKeyPressed(Puluo::Key::RightControl);
            if (ctrl && e.GetKeyCode() == Puluo::Key::S) { SaveScene(); return true; }
            if (ctrl && e.GetKeyCode() == Puluo::Key::O) { LoadScene(); return true; }
            if (ctrl && e.GetKeyCode() == Puluo::Key::Z) { m_CommandHistory.Undo(); return true; }
            if (ctrl && e.GetKeyCode() == Puluo::Key::Y) { m_CommandHistory.Redo(); return true; }
            return false;
        });
    }

    void OnShutdown() override {
        m_InstancedMeshes.clear();
        m_ParticleSystem.Shutdown();
        m_WeatherSystem.Shutdown();
        m_SSR.reset();
        if (m_SceneColorCopy) { glDeleteTextures(1, &m_SceneColorCopy); m_SceneColorCopy = 0; }
        m_CSM.reset();
        m_SSAO.reset();
        m_DepthPrepassFB.reset();
        m_SkyLight.reset();
        m_SceneFB.reset();
        m_PostProcessFB.reset();
        if (m_EmptyVAO) glDeleteVertexArrays(1, &m_EmptyVAO);
        NFD_Quit();
        Puluo::ImGuiLayer::Shutdown();
        Puluo::Renderer::Shutdown();
        PULUO_INFO("Editor shutdown.");
    }

private:
    void ImportModel() {
        nfdu8filteritem_t filters[] = {
            {"3D Models", "glb,gltf,fbx,obj"}
        };
        const nfdpathset_t* pathSet = nullptr;
        nfdresult_t result = NFD_OpenDialogMultipleU8(&pathSet, filters, 1, nullptr);

        if (result == NFD_OKAY && pathSet) {
            nfdpathsetsize_t count = 0;
            NFD_PathSet_GetCount(pathSet, &count);
            for (nfdpathsetsize_t i = 0; i < count; i++) {
                nfdu8char_t* p = nullptr;
                if (NFD_PathSet_GetPath(pathSet, i, &p) == NFD_OKAY && p) {
                    std::string passetPath = Puluo::AssetImporter::ImportModelToProject(p);
                    NFD_PathSet_FreePathU8(p);
                    if (!passetPath.empty()) {
                        ImportModelFromPath(passetPath);
                    }
                }
            }
            NFD_PathSet_Free(pathSet);
        }
    }

    void RebuildInstancedMesh(size_t objIndex) {
        auto& objects = m_Scene.GetObjects();
        if (objIndex >= objects.size()) return;
        auto& obj = objects[objIndex];
        if (!obj.instancedMesh.has_value()) {
            m_InstancedMeshes.erase(objIndex);
            return;
        }
        auto& imd = obj.instancedMesh.value();
        if (imd.modelPath.empty() || imd.instances.empty()) {
            m_InstancedMeshes.erase(objIndex);
            return;
        }
        auto im = std::make_unique<Puluo::InstancedMesh>();
        if (!im->Setup(imd.modelPath, static_cast<uint32_t>(std::max(imd.scatterCount, static_cast<int>(imd.instances.size())) + 256))) {
            m_InstancedMeshes.erase(objIndex);
            return;
        }
        // Convert InstanceTransformData to InstanceTransform
        std::vector<Puluo::InstanceTransform> transforms;
        transforms.reserve(imd.instances.size());
        for (auto& inst : imd.instances) {
            Puluo::InstanceTransform t;
            t.position = inst.position + obj.transform.position; // offset by object position
            t.rotation = inst.rotation;
            t.scale = inst.scale;
            transforms.push_back(t);
        }
        im->SetInstances(transforms);
        m_InstancedMeshes[objIndex] = std::move(im);
    }

    void ImportModelFromPath(const std::string& path) {
        auto model = Puluo::ModelCache::Load(path);
        if (model) {
            std::filesystem::path p(path);
            std::string name = p.stem().string();
            auto& obj = m_Scene.AddObject(name, model);
            obj.modelPath = path;
            m_Scene.Select(m_Scene.GetObjects().size() - 1);
            PULUO_INFO("Imported model: {0}", name);
        } else {
            PULUO_CORE_ERROR("Failed to load model: {0}", path);
        }
    }

    void SaveSceneToPath(const std::string& savePath) {
        // Sync terrain runtime data back to scene object before serialization
        for (auto& obj : m_Scene.GetObjects()) {
            if (obj.terrain.has_value()) {
                auto& td = obj.terrain.value();
                td.created = m_Terrain.IsCreated();
                td.worldSize = m_TerrainParams.worldSize;
                td.heightmapRes = m_TerrainParams.heightmapRes;
                td.patchCount = m_TerrainParams.patchCount;
                td.heightScale = m_TerrainParams.heightScale;
                td.uvScale = m_TerrainParams.uvScale;
                // Sync three-layer material paths
                auto syncLayer = [](Puluo::SceneTerrainLayerPaths& dst, const Puluo::TerrainLayerMaterial& src) {
                    dst.albedoPath = src.albedoPath;
                    dst.normalPath = src.normalPath;
                    dst.roughnessPath = src.roughnessPath;
                    dst.normalStrength = src.normalStrength;
                };
                syncLayer(td.lower, m_TerrainMaterial.lower);
                syncLayer(td.upper, m_TerrainMaterial.upper);
                syncLayer(td.slope, m_TerrainMaterial.slope);
                td.heightThreshold = m_TerrainMaterial.heightThreshold;
                td.slopeThreshold = m_TerrainMaterial.slopeThreshold;
                td.blendSharpness = m_TerrainMaterial.blendSharpness;
                // Save splat map if present
                if (m_Terrain.HasSplatMap()) {
                    std::filesystem::path scenePath(savePath);
                    std::string splatPath = (scenePath.parent_path() / (scenePath.stem().string() + "_splatmap.png")).string();
                    m_Terrain.SaveSplatMap(splatPath);
                    td.splatMapPath = splatPath;
                }
                break;
            }
        }

        nlohmann::json j = m_Scene.ToJson();

        // Environment settings
        nlohmann::json env;

        // Atmosphere
        {
            auto& a = m_AtmosphereParams;
            env["atmosphere"]["enabled"] = m_UseAtmosphere;
            env["atmosphere"]["sunDirection"] = {a.sunDirection.x, a.sunDirection.y, a.sunDirection.z};
            env["atmosphere"]["sunIntensity"] = a.sunIntensity;
            env["atmosphere"]["turbidity"] = a.turbidity;
            env["atmosphere"]["rayleighCoeff"] = {a.rayleighCoeff.x, a.rayleighCoeff.y, a.rayleighCoeff.z};
            env["atmosphere"]["mieCoeff"] = a.mieCoeff;
            env["atmosphere"]["mieDirectionG"] = a.mieDirectionG;
        }

        // Fog
        {
            auto& f = m_FogParams;
            env["fog"]["enabled"] = f.enabled;
            env["fog"]["density"] = f.density;
            env["fog"]["heightFalloff"] = f.heightFalloff;
            env["fog"]["maxOpacity"] = f.maxOpacity;
            env["fog"]["fogColor"] = {f.fogColor.x, f.fogColor.y, f.fogColor.z};
            env["fog"]["startDistance"] = f.startDistance;
            env["fog"]["dirInscatterColor"] = {f.directionalInscatteringColor.x, f.directionalInscatteringColor.y, f.directionalInscatteringColor.z};
            env["fog"]["dirInscatterExp"] = f.directionalInscatteringExponent;
            env["fog"]["dirInscatterStartDist"] = f.directionalInscatteringStartDistance;
        }

        // Cloud
        {
            auto& c = m_CloudParams;
            env["cloud"]["enabled"] = c.enabled;
            env["cloud"]["layerBottom"] = c.cloudLayerBottom;
            env["cloud"]["layerThickness"] = c.cloudLayerThickness;
            env["cloud"]["coverage"] = c.coverage;
            env["cloud"]["density"] = c.density;
            env["cloud"]["detailScale"] = c.detailScale;
            env["cloud"]["baseScale"] = c.baseScale;
            env["cloud"]["windSpeed"] = c.windSpeed;
            env["cloud"]["windDirection"] = {c.windDirection.x, c.windDirection.y, c.windDirection.z};
            env["cloud"]["phaseG"] = c.phaseG;
            env["cloud"]["powderStrength"] = c.powderStrength;
            env["cloud"]["ambientColor"] = {c.ambientColor.x, c.ambientColor.y, c.ambientColor.z};
            env["cloud"]["ambientStrength"] = c.ambientStrength;
        }

        // SSR
        {
            env["ssr"]["enabled"] = m_SSRConfig.enabled;
            env["ssr"]["maxSteps"] = m_SSRConfig.maxSteps;
            env["ssr"]["maxDistance"] = m_SSRConfig.maxDistance;
            env["ssr"]["thickness"] = m_SSRConfig.thickness;
        }

        // SSAO
        {
            env["ssao"]["enabled"] = m_SSAOConfig.enabled;
            env["ssao"]["kernelSize"] = m_SSAOConfig.kernelSize;
            env["ssao"]["radius"] = m_SSAOConfig.radius;
            env["ssao"]["bias"] = m_SSAOConfig.bias;
            env["ssao"]["power"] = m_SSAOConfig.power;
        }

        // Post-processing
        {
            env["postprocess"]["fxaaEnabled"] = m_FXAAEnabled;
            env["postprocess"]["saturation"] = m_Saturation;
            env["postprocess"]["contrast"] = m_Contrast;
        }

        // Weather
        {
            auto& w = m_WeatherConfig;
            env["weather"]["type"] = static_cast<int>(w.type);
            env["weather"]["intensity"] = w.intensity;
            env["weather"]["maxParticles"] = w.maxParticles;
            env["weather"]["areaSize"] = {w.areaSize.x, w.areaSize.y, w.areaSize.z};
            env["weather"]["fallSpeed"] = w.fallSpeed;
            env["weather"]["wind"] = {w.wind.x, w.wind.y, w.wind.z};
            env["weather"]["color"] = {w.color.x, w.color.y, w.color.z, w.color.w};
            env["weather"]["size"] = w.size;
            env["weather"]["streakLength"] = w.streakLength;
        }

        // Camera
        {
            auto& pos = m_Camera.GetPosition();
            env["camera"]["position"] = {pos.x, pos.y, pos.z};
            env["camera"]["yaw"] = m_Camera.GetYaw();
            env["camera"]["pitch"] = m_Camera.GetPitch();
            env["camera"]["moveSpeed"] = m_Camera.GetMoveSpeed();
            env["camera"]["sensitivity"] = m_Camera.GetMouseSensitivity();
            env["camera"]["fov"] = m_Camera.GetFov();
        }

        j["environment"] = env;

        std::ofstream file(savePath);
        if (file.is_open()) {
            file << j.dump(2);
            m_CurrentScenePath = savePath;
            PULUO_INFO("Scene saved: {0}", savePath);
        } else {
            PULUO_CORE_ERROR("Failed to save scene: {0}", savePath);
        }
    }

    void SaveScene() {
        if (!m_CurrentScenePath.empty()) {
            // Quick save to current file
            SaveSceneToPath(m_CurrentScenePath);
        } else {
            SaveSceneAs();
        }
    }

    void SaveSceneAs() {
        std::string defaultDir = std::filesystem::absolute("assets").string();
        nfdu8filteritem_t filters[] = {{"Scene Files", "pscene"}};
        nfdu8char_t* outPath = nullptr;
        if (NFD_SaveDialogU8(&outPath, filters, 1, defaultDir.c_str(), "untitled.pscene") == NFD_OKAY && outPath) {
            SaveSceneToPath(outPath);
            NFD_FreePathU8(outPath);
        }
    }

    void LoadSceneFromPath(const std::string& path) {
        auto j = Puluo::Scene::LoadJsonFromFile(path);
        if (j.is_null()) {
            PULUO_CORE_ERROR("Failed to parse scene: {0}", path);
            return;
        }

        // Rebuild objects
        auto objectsData = Puluo::Scene::FromJson(j);
        m_Scene = Puluo::Scene{};
        m_CommandHistory.Clear();
        m_InstancedMeshes.clear();
        m_Terrain.Destroy();
        m_TerrainMaterial = Puluo::TerrainMaterial{};
        m_LoadedTerrainTextures.clear();
        Puluo::TextureCache::Clear();
        Puluo::ModelCache::Clear();
        for (auto& data : objectsData) {
            if (data.light.has_value()) {
                auto& obj = m_Scene.AddObject(data.name, nullptr);
                obj.transform = data.transform;
                obj.light = data.light;
            } else if (data.particle.has_value()) {
                auto& obj = m_Scene.AddObject(data.name, nullptr);
                obj.transform = data.transform;
                obj.particle = data.particle;
            } else if (data.instancedMesh.has_value()) {
                auto& obj = m_Scene.AddObject(data.name, nullptr);
                obj.transform = data.transform;
                obj.instancedMesh = data.instancedMesh;
                size_t objIdx = m_Scene.GetObjects().size() - 1;
                RebuildInstancedMesh(objIdx);
            } else if (data.terrain.has_value()) {
                auto& obj = m_Scene.AddObject(data.name, nullptr);
                obj.transform = data.transform;
                obj.terrain = data.terrain;
                auto& td = obj.terrain.value();
                // Restore runtime terrain from scene data
                m_TerrainParams.worldSize = td.worldSize;
                m_TerrainParams.heightmapRes = td.heightmapRes;
                m_TerrainParams.patchCount = td.patchCount;
                m_TerrainParams.heightScale = td.heightScale;
                m_TerrainParams.uvScale = td.uvScale;
                m_TerrainMaterial = Puluo::TerrainMaterial{};
                m_LoadedTerrainTextures.clear();
                if (td.created) {
                    m_Terrain.Create(m_TerrainParams);
                    if (!td.heightmapPath.empty()) {
                        m_Terrain.LoadFromImage(td.heightmapPath);
                    } else {
                        m_Terrain.GenerateFromNoise(td.noiseFreq, td.noiseOctaves);
                    }
                }
                // Load material textures
                auto loadTexIfPresent = [this](const std::string& path) -> uint32_t {
                    if (path.empty()) return 0;
                    auto tex = Puluo::TextureCache::Load(path);
                    if (tex && tex->GetRendererID()) {
                        m_LoadedTerrainTextures.push_back(tex);
                        return tex->GetRendererID();
                    }
                    PULUO_CORE_WARN("Failed to load terrain texture: {0}", path);
                    return 0;
                };
                auto loadLayer = [&](Puluo::TerrainLayerMaterial& dst, const Puluo::SceneTerrainLayerPaths& src) {
                    dst.albedoPath = src.albedoPath;
                    dst.normalPath = src.normalPath;
                    dst.roughnessPath = src.roughnessPath;
                    dst.normalStrength = src.normalStrength;
                    dst.albedoTex = loadTexIfPresent(dst.albedoPath);
                    dst.normalTex = loadTexIfPresent(dst.normalPath);
                    dst.roughnessTex = loadTexIfPresent(dst.roughnessPath);
                };
                loadLayer(m_TerrainMaterial.lower, td.lower);
                loadLayer(m_TerrainMaterial.upper, td.upper);
                loadLayer(m_TerrainMaterial.slope, td.slope);
                m_TerrainMaterial.heightThreshold = td.heightThreshold;
                m_TerrainMaterial.slopeThreshold = td.slopeThreshold;
                m_TerrainMaterial.blendSharpness = td.blendSharpness;
                // Load splat map if present
                if (!td.splatMapPath.empty() && m_Terrain.IsCreated()) {
                    m_Terrain.LoadSplatMap(td.splatMapPath);
                }
            } else if (data.water.has_value()) {
                auto& obj = m_Scene.AddObject(data.name, nullptr);
                obj.transform = data.transform;
                obj.water = data.water;
            } else if (!data.modelPath.empty()) {
                auto model = Puluo::ModelCache::Load(data.modelPath);
                if (model) {
                    auto& obj = m_Scene.AddObject(data.name, model);
                    obj.modelPath = data.modelPath;
                    obj.transform = data.transform;
                    // Apply material overrides
                    if (!data.materialOverrides.empty()) {
                        obj.materialOverrides = data.materialOverrides;
                        auto& materials = model->GetMutableMaterials();
                        for (auto& [slot, ovr] : obj.materialOverrides) {
                            if (slot < 0 || slot >= static_cast<int>(materials.size())) continue;
                            auto& mat = materials[slot];
                            mat.albedo = ovr.albedo;
                            mat.metallic = ovr.metallic;
                            mat.roughness = ovr.roughness;
                            mat.ao = ovr.ao;
                            mat.alphaCutoff = ovr.alphaCutoff;
                            mat.useAlphaMask = ovr.useAlphaMask;
                            mat.useSSS = ovr.useSSS;
                            mat.sssColor = ovr.sssColor;
                            mat.sssStrength = ovr.sssStrength;
                            if (!ovr.albedoMapPath.empty())
                                mat.albedoMap = Puluo::TextureCache::Load(ovr.albedoMapPath);
                            if (!ovr.normalMapPath.empty())
                                mat.normalMap = Puluo::TextureCache::Load(ovr.normalMapPath);
                            if (!ovr.metallicMapPath.empty())
                                mat.metallicMap = Puluo::TextureCache::Load(ovr.metallicMapPath);
                            if (!ovr.roughnessMapPath.empty())
                                mat.roughnessMap = Puluo::TextureCache::Load(ovr.roughnessMapPath);
                            if (!ovr.aoMapPath.empty())
                                mat.aoMap = Puluo::TextureCache::Load(ovr.aoMapPath);
                            if (!ovr.maskMapPath.empty())
                                mat.maskMap = Puluo::TextureCache::Load(ovr.maskMapPath);
                        }
                    }
                } else {
                    PULUO_CORE_WARN("Failed to load model for '{0}': {1}", data.name, data.modelPath);
                }
            }
        }

        // Load environment (v2+, optional for backward compat)
        if (j.contains("environment")) {
            auto& env = j["environment"];

            // Atmosphere
            if (env.contains("atmosphere")) {
                auto& a = env["atmosphere"];
                m_UseAtmosphere = a.value("enabled", false);
                if (a.contains("sunDirection")) {
                    auto& sd = a["sunDirection"];
                    m_AtmosphereParams.sunDirection = {sd[0].get<float>(), sd[1].get<float>(), sd[2].get<float>()};
                }
                m_AtmosphereParams.sunIntensity = a.value("sunIntensity", 22.0f);
                m_AtmosphereParams.turbidity = a.value("turbidity", 1.5f);
                if (a.contains("rayleighCoeff")) {
                    auto& rc = a["rayleighCoeff"];
                    m_AtmosphereParams.rayleighCoeff = {rc[0].get<float>(), rc[1].get<float>(), rc[2].get<float>()};
                }
                m_AtmosphereParams.mieCoeff = a.value("mieCoeff", 21e-6f);
                m_AtmosphereParams.mieDirectionG = a.value("mieDirectionG", 0.93f);
            }

            // Fog
            if (env.contains("fog")) {
                auto& f = env["fog"];
                m_FogParams.enabled = f.value("enabled", false);
                m_FogParams.density = f.value("density", 0.02f);
                m_FogParams.heightFalloff = f.value("heightFalloff", 0.2f);
                m_FogParams.maxOpacity = f.value("maxOpacity", 1.0f);
                if (f.contains("fogColor")) {
                    auto& fc = f["fogColor"];
                    m_FogParams.fogColor = {fc[0].get<float>(), fc[1].get<float>(), fc[2].get<float>()};
                }
                m_FogParams.startDistance = f.value("startDistance", 10.0f);
                if (f.contains("dirInscatterColor")) {
                    auto& dc = f["dirInscatterColor"];
                    m_FogParams.directionalInscatteringColor = {dc[0].get<float>(), dc[1].get<float>(), dc[2].get<float>()};
                }
                m_FogParams.directionalInscatteringExponent = f.value("dirInscatterExp", 4.0f);
                m_FogParams.directionalInscatteringStartDistance = f.value("dirInscatterStartDist", 20.0f);
            }

            // Cloud
            if (env.contains("cloud")) {
                auto& c = env["cloud"];
                m_CloudParams.enabled = c.value("enabled", false);
                m_CloudParams.cloudLayerBottom = c.value("layerBottom", 4000.0f);
                m_CloudParams.cloudLayerThickness = c.value("layerThickness", 4500.0f);
                m_CloudParams.coverage = c.value("coverage", 0.5f);
                m_CloudParams.density = c.value("density", 0.001f);
                m_CloudParams.detailScale = c.value("detailScale", 0.001f);
                m_CloudParams.baseScale = c.value("baseScale", 0.00008f);
                m_CloudParams.windSpeed = c.value("windSpeed", 5.0f);
                if (c.contains("windDirection")) {
                    auto& wd = c["windDirection"];
                    m_CloudParams.windDirection = {wd[0].get<float>(), wd[1].get<float>(), wd[2].get<float>()};
                }
                m_CloudParams.phaseG = c.value("phaseG", 0.35f);
                m_CloudParams.powderStrength = c.value("powderStrength", 2.0f);
                if (c.contains("ambientColor")) {
                    auto& ac = c["ambientColor"];
                    m_CloudParams.ambientColor = {ac[0].get<float>(), ac[1].get<float>(), ac[2].get<float>()};
                }
                m_CloudParams.ambientStrength = c.value("ambientStrength", 0.15f);
            }

            // SSR
            if (env.contains("ssr")) {
                auto& s = env["ssr"];
                m_SSRConfig.enabled = s.value("enabled", false);
                m_SSRConfig.maxSteps = s.value("maxSteps", 64);
                m_SSRConfig.maxDistance = s.value("maxDistance", 50.0f);
                m_SSRConfig.thickness = s.value("thickness", 0.5f);
            }

            // SSAO
            if (env.contains("ssao")) {
                auto& s = env["ssao"];
                m_SSAOConfig.enabled = s.value("enabled", true);
                m_SSAOConfig.kernelSize = s.value("kernelSize", 32u);
                m_SSAOConfig.radius = s.value("radius", 0.5f);
                m_SSAOConfig.bias = s.value("bias", 0.025f);
                m_SSAOConfig.power = s.value("power", 2.0f);
            }

            // Post-processing
            if (env.contains("postprocess")) {
                auto& pp = env["postprocess"];
                m_FXAAEnabled = pp.value("fxaaEnabled", true);
                m_Saturation = pp.value("saturation", 1.0f);
                m_Contrast = pp.value("contrast", 1.0f);
            }

            // Weather
            if (env.contains("weather")) {
                auto& w = env["weather"];
                m_WeatherConfig.type = static_cast<Puluo::WeatherType>(w.value("type", 0));
                m_WeatherConfig.intensity = w.value("intensity", 0.5f);
                m_WeatherConfig.maxParticles = w.value("maxParticles", 30000);
                if (w.contains("areaSize")) {
                    auto& a = w["areaSize"];
                    m_WeatherConfig.areaSize = {a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
                }
                m_WeatherConfig.fallSpeed = w.value("fallSpeed", 12.0f);
                if (w.contains("wind")) {
                    auto& wi = w["wind"];
                    m_WeatherConfig.wind = {wi[0].get<float>(), wi[1].get<float>(), wi[2].get<float>()};
                }
                if (w.contains("color")) {
                    auto& c = w["color"];
                    m_WeatherConfig.color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
                }
                m_WeatherConfig.size = w.value("size", 0.05f);
                m_WeatherConfig.streakLength = w.value("streakLength", 0.4f);
            }

            // Terrain (backward compat: old format stored terrain in env)
            if (env.contains("terrain")) {
                // Check if terrain scene object was already loaded
                bool hasTerrainObj = false;
                for (auto& obj : m_Scene.GetObjects()) {
                    if (obj.terrain.has_value()) { hasTerrainObj = true; break; }
                }
                if (!hasTerrainObj) {
                    auto& t = env["terrain"];
                    bool wasCreated = t.value("created", false);
                    Puluo::SceneTerrainData td;
                    td.worldSize = t.value("worldSize", 500.0f);
                    td.heightmapRes = t.value("heightmapRes", 257);
                    td.patchCount = t.value("patchCount", 64);
                    td.heightScale = t.value("heightScale", 80.0f);
                    td.uvScale = t.value("uvScale", 50.0f);
                    td.created = wasCreated;

                    m_TerrainParams.worldSize = td.worldSize;
                    m_TerrainParams.heightmapRes = td.heightmapRes;
                    m_TerrainParams.patchCount = td.patchCount;
                    m_TerrainParams.heightScale = td.heightScale;
                    m_TerrainParams.uvScale = td.uvScale;

                    if (wasCreated) {
                        m_Terrain.Create(m_TerrainParams);
                        m_Terrain.GenerateFromNoise();
                    }

                    // Material texture paths (backward compat: old single material → lower layer)
                    m_TerrainMaterial = Puluo::TerrainMaterial{};
                    m_LoadedTerrainTextures.clear();
                    if (t.contains("material")) {
                        auto& mat = t["material"];
                        auto loadTexIfPresent = [this](const std::string& path) -> uint32_t {
                            if (path.empty()) return 0;
                            auto tex = Puluo::TextureCache::Load(path);
                            if (tex && tex->GetRendererID()) {
                                m_LoadedTerrainTextures.push_back(tex);
                                return tex->GetRendererID();
                            }
                            PULUO_CORE_WARN("Failed to load terrain texture: {0}", path);
                            return 0;
                        };
                        // Old format only had one set → put into lower layer
                        m_TerrainMaterial.lower.albedoPath = mat.value("albedoPath", "");
                        m_TerrainMaterial.lower.normalPath = mat.value("normalPath", "");
                        m_TerrainMaterial.lower.roughnessPath = mat.value("roughnessPath", "");
                        td.lower.albedoPath = m_TerrainMaterial.lower.albedoPath;
                        td.lower.normalPath = m_TerrainMaterial.lower.normalPath;
                        td.lower.roughnessPath = m_TerrainMaterial.lower.roughnessPath;
                        m_TerrainMaterial.lower.albedoTex = loadTexIfPresent(m_TerrainMaterial.lower.albedoPath);
                        m_TerrainMaterial.lower.normalTex = loadTexIfPresent(m_TerrainMaterial.lower.normalPath);
                        m_TerrainMaterial.lower.roughnessTex = loadTexIfPresent(m_TerrainMaterial.lower.roughnessPath);
                    }

                    // Create terrain scene object for new format
                    auto& terrainObj = m_Scene.AddObject("Terrain", nullptr);
                    terrainObj.terrain = td;
                }
            }

            // Camera
            if (env.contains("camera")) {
                auto& cam = env["camera"];
                Puluo::Vec3 pos{0.0f, 1.0f, 5.0f};
                if (cam.contains("position")) {
                    auto& p = cam["position"];
                    pos = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                }
                m_Camera.RestoreState(
                    pos,
                    cam.value("yaw", -90.0f),
                    cam.value("pitch", 0.0f),
                    cam.value("fov", 45.0f),
                    cam.value("moveSpeed", 5.0f),
                    cam.value("sensitivity", 0.1f)
                );
            }
        }

        m_CurrentScenePath = path;
        PULUO_INFO("Scene loaded: {0}", path);
    }

    void LoadScene() {
        std::string defaultDir = std::filesystem::absolute("assets").string();
        nfdu8filteritem_t filters[] = {{"Scene Files", "pscene"}};
        nfdu8char_t* outPath = nullptr;
        if (NFD_OpenDialogU8(&outPath, filters, 1, defaultDir.c_str()) == NFD_OKAY && outPath) {
            LoadSceneFromPath(outPath);
            NFD_FreePathU8(outPath);
        }
    }

    std::shared_ptr<Puluo::Shader> m_PBRShader;
    std::shared_ptr<Puluo::Shader> m_SkyboxShader;
    std::shared_ptr<Puluo::Shader> m_AtmosphereShader;
    std::shared_ptr<Puluo::Shader> m_LineShader;
    std::unique_ptr<Puluo::Framebuffer> m_SceneFB;
    std::unique_ptr<Puluo::Framebuffer> m_PostProcessFB;
    std::shared_ptr<Puluo::Shader> m_FXAAShader;
    uint32_t m_EmptyVAO = 0;
    bool m_FXAAEnabled = true;
    float m_Saturation = 1.0f;
    float m_Contrast = 1.0f;
    Puluo::CameraController m_Camera;
    Puluo::LightManager m_Lights;
    Puluo::IBLMaps m_IBLMaps;

    // Atmosphere
    Puluo::AtmosphereParams m_AtmosphereParams;
    bool m_UseAtmosphere = false;
    std::unique_ptr<Puluo::SkyLight> m_SkyLight;

    // Fog
    Puluo::FogParams m_FogParams;

    // Volumetric Clouds
    Puluo::CloudParams m_CloudParams;
    std::shared_ptr<Puluo::Shader> m_CloudShader;
    uint32_t m_BaseNoiseTexture = 0;
    uint32_t m_DetailNoiseTexture = 0;
    float m_CloudTime = 0.0f;

    // Terrain
    Puluo::Terrain m_Terrain;
    Puluo::TerrainParams m_TerrainParams;
    Puluo::TerrainMaterial m_TerrainMaterial;
    std::shared_ptr<Puluo::Shader> m_TerrainShader;
    std::vector<std::shared_ptr<Puluo::Texture2D>> m_LoadedTerrainTextures; // keep alive

    // CSM Shadows
    std::unique_ptr<Puluo::CascadedShadowMap> m_CSM;
    std::shared_ptr<Puluo::Shader> m_ShadowModelShader;
    std::shared_ptr<Puluo::Shader> m_ShadowTerrainShader;
    Puluo::CSMConfig m_CSMConfig;

    // Render passes
    Puluo::ShadowPass m_ShadowPass;
    Puluo::DepthPrepassPass m_DepthPrepassPass;
    Puluo::SSAOPass m_SSAOPass;
    Puluo::PostProcessPass m_PostProcessPass;
    Puluo::RenderContext m_RenderCtx;

    // SSAO
    std::unique_ptr<Puluo::SSAO> m_SSAO;
    Puluo::SSAOConfig m_SSAOConfig;
    std::unique_ptr<Puluo::Framebuffer> m_DepthPrepassFB;
    std::shared_ptr<Puluo::Shader> m_DepthPrepassModelShader;
    std::shared_ptr<Puluo::Shader> m_DepthPrepassTerrainShader;

    // Particle System
    Puluo::ParticleSystem m_ParticleSystem;
    float m_DeltaTime = 0.0f;

    // Weather System
    Puluo::WeatherSystem m_WeatherSystem;
    Puluo::WeatherConfig m_WeatherConfig;

    // Instanced Mesh
    std::shared_ptr<Puluo::Shader> m_PBRInstancedShader;
    std::shared_ptr<Puluo::Shader> m_ShadowModelInstancedShader;
    std::shared_ptr<Puluo::Shader> m_DepthPrepassInstancedShader;
    std::unordered_map<size_t, std::unique_ptr<Puluo::InstancedMesh>> m_InstancedMeshes;
    float m_InstancedMaxDrawDistance = 1000.0f; // 0 = unlimited

    // SSR
    std::unique_ptr<Puluo::SSR> m_SSR;
    Puluo::SSRConfig m_SSRConfig;

    // Water
    std::shared_ptr<Puluo::Shader> m_WaterShader;
    Puluo::Mesh m_WaterMesh;
    uint32_t m_SceneColorCopy = 0;
    uint32_t m_SceneColorCopyWidth = 0;
    uint32_t m_SceneColorCopyHeight = 0;

    // Grid and axes
    std::shared_ptr<Puluo::VertexArray> m_GridVAO;
    uint32_t m_GridVertexCount = 0;
    std::shared_ptr<Puluo::VertexArray> m_AxisVAO;
    uint32_t m_AxisVertexCount = 0;

    // Editor state
    Puluo::Scene m_Scene;
    Puluo::CommandHistory m_CommandHistory;
    Puluo::GizmoMode m_GizmoMode = Puluo::GizmoMode::Translate;
    std::string m_CurrentScenePath;  // Path of currently open scene (empty = unsaved)
    bool m_FirstFrame = true;
    bool m_ViewportHovered = false;
    bool m_ViewportFocused = false;
    bool m_GizmoWasUsing = false;
    bool m_ShowStatsOverlay = true;
    Puluo::Transform m_GizmoStartTransform;
};

Puluo::Application* Puluo::CreateApplication() {
    return new SandboxApp();
}
