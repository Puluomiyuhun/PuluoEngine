#include "puluo/renderer/WeatherSystem.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <random>
#include <cmath>
#include <algorithm>

namespace Puluo {

static thread_local std::mt19937 s_WeatherRNG{std::random_device{}()};

static float RandFloat(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(s_WeatherRNG);
}

void WeatherSystem::Init() {
    // Unit quad: 6 vertices (2 triangles)
    float quadVerts[] = {
        // pos        uv
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };

    m_QuadVBO = std::make_shared<VertexBuffer>(quadVerts, sizeof(quadVerts));
    m_QuadVBO->SetLayout({
        {"aQuadPos",  ShaderDataType::Float2},
        {"aTexCoord", ShaderDataType::Float2}
    });

    m_Shader = Shader::CreateFromFile("assets/shaders/weather.vert",
                                       "assets/shaders/weather.frag");
    if (!m_Shader)
        PULUO_CORE_ERROR("Failed to load weather shaders!");

    m_Initialized = true;
    PULUO_CORE_INFO("WeatherSystem initialized");
}

void WeatherSystem::Shutdown() {
    m_VAO.reset();
    m_QuadVBO.reset();
    m_InstanceVBO.reset();
    m_Shader.reset();
    m_Particles.clear();
    m_Instances.clear();
    m_Initialized = false;
}

void WeatherSystem::EnsureCapacity(int maxParticles) {
    if (maxParticles <= m_AllocatedMax) return;

    m_AllocatedMax = maxParticles;
    m_Particles.resize(maxParticles);
    m_Instances.resize(maxParticles);

    // Recreate instance VBO and VAO
    uint32_t bufSize = maxParticles * static_cast<uint32_t>(sizeof(WeatherInstanceData));
    m_InstanceVBO = std::make_shared<VertexBuffer>(bufSize);
    m_InstanceVBO->SetLayout({
        {"aWorldPos", ShaderDataType::Float3},
        {"aOpacity",  ShaderDataType::Float}
    });

    m_VAO = std::make_shared<VertexArray>();
    m_VAO->AddVertexBuffer(m_QuadVBO);
    m_VAO->AddInstanceBuffer(m_InstanceVBO);
}

void WeatherSystem::RespawnParticle(WeatherParticle& p, const Vec3& cameraPos,
                                     const Vec3& areaSize, bool randomY) {
    float halfX = areaSize.x * 0.5f;
    float halfZ = areaSize.z * 0.5f;
    float halfY = areaSize.y * 0.5f;

    p.position.x = cameraPos.x + RandFloat(-halfX, halfX);
    p.position.z = cameraPos.z + RandFloat(-halfZ, halfZ);

    if (randomY)
        p.position.y = cameraPos.y + RandFloat(-halfY, halfY);
    else
        p.position.y = cameraPos.y + RandFloat(halfY * 0.5f, halfY);

    p.phase = RandFloat(0.0f, 6.2831853f);
}

void WeatherSystem::Update(float dt, const Vec3& cameraPos) {
    m_Time += dt;
    m_DeltaTime = dt;
    m_LastCameraPos = cameraPos;
}

void WeatherSystem::Render(const CameraController& camera, const WeatherConfig& config) {
    if (!m_Initialized || !m_Shader) return;
    if (config.type == WeatherType::None || config.intensity <= 0.0f) return;

    EnsureCapacity(config.maxParticles);

    int targetActive = static_cast<int>(config.intensity * config.maxParticles);
    targetActive = std::clamp(targetActive, 0, config.maxParticles);

    const Vec3& camPos = m_LastCameraPos;
    const Vec3& area = config.areaSize;
    float halfX = area.x * 0.5f;
    float halfY = area.y * 0.5f;
    float halfZ = area.z * 0.5f;

    // Spawn new particles if we need more
    while (m_ActiveCount < targetActive) {
        RespawnParticle(m_Particles[m_ActiveCount], camPos, area, true);
        m_ActiveCount++;
    }
    // Shrink if intensity decreased
    if (m_ActiveCount > targetActive)
        m_ActiveCount = targetActive;

    if (m_ActiveCount == 0) return;

    // Update positions + build instance data
    float dt = m_DeltaTime > 0.0f ? m_DeltaTime : (1.0f / 60.0f);

    bool isSnow = (config.type == WeatherType::Snow);

    for (int i = 0; i < m_ActiveCount; i++) {
        auto& p = m_Particles[i];

        // Apply gravity + wind
        p.position.y -= config.fallSpeed * dt;
        p.position.x += config.wind.x * dt;
        p.position.z += config.wind.z * dt;

        // Snow wobble
        if (isSnow) {
            p.position.x += std::sin(m_Time * 2.0f + p.phase) * 0.5f * dt;
            p.position.z += std::cos(m_Time * 1.7f + p.phase * 1.3f) * 0.3f * dt;
        }

        // Wrap: recycle particles that leave the volume
        Vec3 offset = p.position - camPos;
        bool outOfBounds = (offset.y < -halfY) ||
                           (std::abs(offset.x) > halfX) ||
                           (std::abs(offset.z) > halfZ);

        if (outOfBounds) {
            RespawnParticle(p, camPos, area, false);
        }

        // Edge fade: particles near boundary edges fade out
        float fadeX = 1.0f - std::max(0.0f, (std::abs(offset.x) - halfX * 0.7f) / (halfX * 0.3f));
        float fadeZ = 1.0f - std::max(0.0f, (std::abs(offset.z) - halfZ * 0.7f) / (halfZ * 0.3f));
        float fadeY = 1.0f - std::max(0.0f, (std::abs(offset.y) - halfY * 0.7f) / (halfY * 0.3f));

        m_Instances[i].position = p.position;
        m_Instances[i].opacity = std::clamp(fadeX * fadeZ * fadeY, 0.0f, 1.0f);
    }

    // Upload to GPU
    uint32_t uploadSize = m_ActiveCount * static_cast<uint32_t>(sizeof(WeatherInstanceData));
    m_InstanceVBO->SetData(m_Instances.data(), uploadSize);

    // Render
    Mat4 view = camera.GetViewMatrix();
    Vec3 cameraRight = Vec3(view[0][0], view[1][0], view[2][0]);
    Vec3 cameraUp    = Vec3(view[0][1], view[1][1], view[2][1]);

    // Fall direction = gravity + wind (normalized)
    Vec3 fallDir = Vec3(config.wind.x, -config.fallSpeed, config.wind.z);
    float fallLen = glm::length(fallDir);
    if (fallLen > 0.001f) fallDir /= fallLen;
    else fallDir = Vec3(0.0f, -1.0f, 0.0f);

    m_Shader->Bind();
    m_Shader->SetMat4("uViewProjection", camera.GetViewProjection());
    m_Shader->SetVec3("uCameraRight", cameraRight);
    m_Shader->SetVec3("uCameraUp", cameraUp);
    m_Shader->SetInt("uWeatherType", config.type == WeatherType::Rain ? 0 : 1);
    m_Shader->SetFloat("uSize", config.size);
    m_Shader->SetFloat("uStreakLength", config.streakLength);
    m_Shader->SetVec3("uFallDirection", fallDir);
    m_Shader->SetVec4("uColor", config.color);

    RenderCommand::SetDepthWrite(false);
    RenderCommand::SetBlending(true);
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderCommand::DrawArraysInstanced(m_VAO, 6, static_cast<uint32_t>(m_ActiveCount));

    RenderCommand::SetBlending(false);
    RenderCommand::SetDepthWrite(true);
}

} // namespace Puluo
