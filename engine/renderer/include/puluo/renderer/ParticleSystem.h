#pragma once

#include "puluo/renderer/Particle.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/Buffer.h"
#include "puluo/renderer/VertexArray.h"
#include "puluo/renderer/Camera.h"
#include <vector>
#include <memory>
#include <unordered_map>

namespace Puluo {

class ParticleSystem {
public:
    ParticleSystem() = default;
    ~ParticleSystem() = default;

    void Init();
    void Shutdown();

    // Call at the start of each render frame to prepare emission
    void BeginFrame();

    // Emit particles from a world position with given params
    // emitterID: unique identifier for accumulator tracking (e.g. scene object index)
    void EmitFrom(uint32_t emitterID, const Vec3& worldPosition,
                  const ParticleEmitterParams& params, float dt);

    // Simulate all particles (call once per frame after all EmitFrom calls)
    void Update(float dt);

    // Render all particles
    void Render(const CameraController& cameraCtrl);

private:
    struct Particle {
        Vec3 position;
        Vec3 velocity;
        float lifetime;      // remaining
        float maxLifetime;   // initial
        float sizeStart;
        float sizeEnd;
        Vec4 colorStart;
        Vec4 colorEnd;
        Vec3 gravity;
        float drag;
        ParticleBlendMode blendMode;
    };

    // Per-instance GPU data (tightly packed)
    struct ParticleInstanceData {
        Vec3 position;   // 12 bytes
        float size;      //  4 bytes
        Vec4 color;      // 16 bytes
    };  // 32 bytes total

    std::vector<Particle> m_Particles;
    std::vector<ParticleInstanceData> m_AlphaInstances;
    std::vector<ParticleInstanceData> m_AdditiveInstances;

    // Emission accumulators per emitter
    std::unordered_map<uint32_t, float> m_EmissionAccumulators;

    // GPU resources
    std::shared_ptr<VertexArray> m_VAO;
    std::shared_ptr<VertexBuffer> m_QuadVBO;
    std::shared_ptr<VertexBuffer> m_InstanceVBO;
    std::shared_ptr<Shader> m_Shader;

    static constexpr int MAX_PARTICLES = 10000;

    void CreateQuadVAO();
    void SortAndBuildInstances(const Vec3& cameraPos);
    void RenderBatch(const std::vector<ParticleInstanceData>& instances);
};

} // namespace Puluo
