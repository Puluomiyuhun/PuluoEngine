#include "puluo/renderer/ParticleSystem.h"
#include "puluo/renderer/RenderCommand.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <algorithm>
#include <random>
#include <cmath>

namespace Puluo {

// ---- Presets ----

std::vector<std::string> GetParticlePresetNames() {
    return {"Fire", "Smoke", "Sparks", "Dust"};
}

ParticleEmitterParams GetParticlePreset(const std::string& name) {
    ParticleEmitterParams p;
    p.presetName = name;

    if (name == "Fire") {
        p.emissionRate = 80.0f;
        p.maxParticles = 500;
        p.velocityMin  = {-0.3f, 1.5f, -0.3f};
        p.velocityMax  = { 0.3f, 3.0f,  0.3f};
        p.lifetimeMin  = 0.5f;
        p.lifetimeMax  = 1.5f;
        p.sizeStart    = 0.3f;
        p.sizeEnd      = 0.05f;
        p.colorStart   = {1.0f, 0.6f, 0.1f, 1.0f};
        p.colorEnd     = {0.8f, 0.1f, 0.0f, 0.0f};
        p.gravity      = {0.0f, 1.0f, 0.0f};
        p.drag         = 0.3f;
        p.blendMode    = ParticleBlendMode::Additive;
    } else if (name == "Smoke") {
        p.emissionRate = 30.0f;
        p.maxParticles = 300;
        p.velocityMin  = {-0.2f, 0.5f, -0.2f};
        p.velocityMax  = { 0.2f, 2.0f,  0.2f};
        p.lifetimeMin  = 2.0f;
        p.lifetimeMax  = 5.0f;
        p.sizeStart    = 0.1f;
        p.sizeEnd      = 0.8f;
        p.colorStart   = {0.4f, 0.4f, 0.4f, 0.6f};
        p.colorEnd     = {0.2f, 0.2f, 0.2f, 0.0f};
        p.gravity      = {0.0f, 0.5f, 0.0f};
        p.drag         = 0.8f;
        p.blendMode    = ParticleBlendMode::Alpha;
    } else if (name == "Sparks") {
        p.emissionRate = 120.0f;
        p.maxParticles = 800;
        p.velocityMin  = {-2.0f, 2.0f, -2.0f};
        p.velocityMax  = { 2.0f, 5.0f,  2.0f};
        p.lifetimeMin  = 0.3f;
        p.lifetimeMax  = 0.8f;
        p.sizeStart    = 0.08f;
        p.sizeEnd      = 0.02f;
        p.colorStart   = {1.0f, 0.9f, 0.5f, 1.0f};
        p.colorEnd     = {1.0f, 0.4f, 0.0f, 0.0f};
        p.gravity      = {0.0f, -9.81f, 0.0f};
        p.drag         = 0.1f;
        p.blendMode    = ParticleBlendMode::Additive;
    } else if (name == "Dust") {
        p.emissionRate = 10.0f;
        p.maxParticles = 200;
        p.velocityMin  = {-0.3f, 0.05f, -0.3f};
        p.velocityMax  = { 0.3f, 0.3f,   0.3f};
        p.lifetimeMin  = 3.0f;
        p.lifetimeMax  = 8.0f;
        p.sizeStart    = 0.03f;
        p.sizeEnd      = 0.06f;
        p.colorStart   = {0.7f, 0.65f, 0.5f, 0.4f};
        p.colorEnd     = {0.5f, 0.45f, 0.35f, 0.0f};
        p.gravity      = {0.0f, 0.02f, 0.0f};
        p.drag         = 0.95f;
        p.blendMode    = ParticleBlendMode::Alpha;
    }

    return p;
}

// ---- Thread-local random engine ----

static thread_local std::mt19937 s_RNG{std::random_device{}()};

static float RandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(s_RNG);
}

static Vec3 RandomVec3(const Vec3& min, const Vec3& max) {
    return {RandomFloat(min.x, max.x),
            RandomFloat(min.y, max.y),
            RandomFloat(min.z, max.z)};
}

// ---- ParticleSystem ----

void ParticleSystem::Init() {
    m_Particles.reserve(MAX_PARTICLES);
    m_AlphaInstances.reserve(MAX_PARTICLES);
    m_AdditiveInstances.reserve(MAX_PARTICLES);

    CreateQuadVAO();

    m_Shader = Shader::CreateFromFile("assets/shaders/particle.vert",
                                       "assets/shaders/particle.frag");
    if (!m_Shader) {
        PULUO_CORE_ERROR("Failed to load particle shaders!");
    }
}

void ParticleSystem::Shutdown() {
    m_VAO.reset();
    m_QuadVBO.reset();
    m_InstanceVBO.reset();
    m_Shader.reset();
    m_Particles.clear();
    m_EmissionAccumulators.clear();
}

void ParticleSystem::CreateQuadVAO() {
    // Unit quad: 2 triangles, 6 vertices
    // Each vertex: position (vec2) + texCoord (vec2)
    float quadVertices[] = {
        // pos          // uv
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,

        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f,
    };

    m_QuadVBO = std::make_shared<VertexBuffer>(quadVertices, sizeof(quadVertices));
    m_QuadVBO->SetLayout({
        {"aQuadPos",  ShaderDataType::Float2},
        {"aTexCoord", ShaderDataType::Float2}
    });

    // Dynamic instance buffer (pre-allocate for max particles)
    uint32_t instanceBufferSize = MAX_PARTICLES * sizeof(ParticleInstanceData);
    m_InstanceVBO = std::make_shared<VertexBuffer>(instanceBufferSize);
    m_InstanceVBO->SetLayout({
        {"aWorldPos", ShaderDataType::Float3},
        {"aSize",     ShaderDataType::Float},
        {"aColor",    ShaderDataType::Float4}
    });

    m_VAO = std::make_shared<VertexArray>();
    m_VAO->AddVertexBuffer(m_QuadVBO);
    m_VAO->AddInstanceBuffer(m_InstanceVBO);
}

void ParticleSystem::BeginFrame() {
    // Nothing to clear per-frame; accumulators persist
}

void ParticleSystem::EmitFrom(uint32_t emitterID, const Vec3& worldPosition,
                               const ParticleEmitterParams& params, float dt) {
    if (!params.enabled || params.emissionRate <= 0.0f) return;

    float& accum = m_EmissionAccumulators[emitterID];
    accum += params.emissionRate * dt;

    while (accum >= 1.0f && static_cast<int>(m_Particles.size()) < MAX_PARTICLES) {
        Particle p;
        p.position    = worldPosition;
        p.velocity    = RandomVec3(params.velocityMin, params.velocityMax);
        p.maxLifetime = RandomFloat(params.lifetimeMin, params.lifetimeMax);
        p.lifetime    = p.maxLifetime;
        p.sizeStart   = params.sizeStart;
        p.sizeEnd     = params.sizeEnd;
        p.colorStart  = params.colorStart;
        p.colorEnd    = params.colorEnd;
        p.gravity     = params.gravity;
        p.drag        = params.drag;
        p.blendMode   = params.blendMode;

        m_Particles.push_back(p);
        accum -= 1.0f;
    }

    // Clamp accumulator to prevent burst after lag spike
    if (accum > 5.0f) accum = 5.0f;
}
  
void ParticleSystem::Update(float dt) {
    for (auto& p : m_Particles) {
        // Apply gravity
        p.velocity += p.gravity * dt;

        // Apply drag
        p.velocity *= (1.0f - p.drag * dt);

        // Integrate position
        p.position += p.velocity * dt;

        // Decrease lifetime
        p.lifetime -= dt;
    }

    // Remove dead particles (erase-remove idiom)
    m_Particles.erase(
        std::remove_if(m_Particles.begin(), m_Particles.end(),
                        [](const Particle& p) { return p.lifetime <= 0.0f; }),
        m_Particles.end()
    );
}

void ParticleSystem::SortAndBuildInstances(const Vec3& cameraPos) {
    m_AlphaInstances.clear();
    m_AdditiveInstances.clear();

    // Build temporary array with distance for sorting
    struct ParticleWithDist {
        const Particle* particle;
        float distSq;
    };

    std::vector<ParticleWithDist> sortable;
    sortable.reserve(m_Particles.size());

    for (const auto& p : m_Particles) {
        Vec3 diff = p.position - cameraPos;
        float distSq = glm::dot(diff, diff);
        sortable.push_back({&p, distSq});
    }

    // Sort back-to-front (furthest first)
    std::sort(sortable.begin(), sortable.end(),
              [](const ParticleWithDist& a, const ParticleWithDist& b) {
                  return a.distSq > b.distSq;
              });

    // Build instance data, split by blend mode
    for (const auto& s : sortable) {
        const Particle& p = *s.particle;
        float t = 1.0f - (p.lifetime / p.maxLifetime); // 0 at birth, 1 at death

        ParticleInstanceData inst;
        inst.position = p.position;
        inst.size     = glm::mix(p.sizeStart, p.sizeEnd, t);
        inst.color    = glm::mix(p.colorStart, p.colorEnd, t);

        if (p.blendMode == ParticleBlendMode::Alpha) {
            m_AlphaInstances.push_back(inst);
        } else {
            m_AdditiveInstances.push_back(inst);
        }
    }
}

void ParticleSystem::RenderBatch(const std::vector<ParticleInstanceData>& instances) {
    if (instances.empty()) return;

    uint32_t uploadSize = static_cast<uint32_t>(instances.size() * sizeof(ParticleInstanceData));
    m_InstanceVBO->SetData(instances.data(), uploadSize);

    RenderCommand::DrawArraysInstanced(m_VAO, 6, static_cast<uint32_t>(instances.size()));
}

void ParticleSystem::Render(const CameraController& cameraCtrl) {
    if (m_Particles.empty() || !m_Shader) return;

    SortAndBuildInstances(cameraCtrl.GetPosition());

    if (m_AlphaInstances.empty() && m_AdditiveInstances.empty()) return;

    // Extract camera right/up from view matrix for billboarding
    Mat4 view = cameraCtrl.GetViewMatrix();
    Vec3 cameraRight = Vec3(view[0][0], view[1][0], view[2][0]);
    Vec3 cameraUp    = Vec3(view[0][1], view[1][1], view[2][1]);

    m_Shader->Bind();
    m_Shader->SetMat4("uViewProjection", cameraCtrl.GetViewProjection());
    m_Shader->SetVec3("uCameraRight", cameraRight);
    m_Shader->SetVec3("uCameraUp", cameraUp);

    // Disable depth write (particles read depth but don't write)
    RenderCommand::SetDepthWrite(false);
    RenderCommand::SetBlending(true);

    // Pass 1: Alpha-blended particles
    if (!m_AlphaInstances.empty()) {
        RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderBatch(m_AlphaInstances);
    }

    // Pass 2: Additive particles
    if (!m_AdditiveInstances.empty()) {
        RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
        RenderBatch(m_AdditiveInstances);
    }

    // Restore state
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    RenderCommand::SetDepthWrite(true);
}

} // namespace Puluo
