#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/Buffer.h"
#include "puluo/renderer/VertexArray.h"
#include "puluo/renderer/Camera.h"

#include <vector>
#include <memory>

namespace Puluo {

enum class WeatherType : int { None = 0, Rain = 1, Snow = 2 };

struct WeatherConfig {
    WeatherType type = WeatherType::None;
    float intensity = 0.5f;           // 0~1, controls active particle count
    int maxParticles = 30000;
    Vec3 areaSize{60.0f, 30.0f, 60.0f}; // spawn volume around camera
    float fallSpeed = 12.0f;          // rain~12, snow~1.5
    Vec3 wind{0.0f, 0.0f, 0.0f};
    Vec4 color{0.7f, 0.8f, 0.9f, 0.3f};
    float size = 0.05f;
    float streakLength = 0.4f;        // rain streak length
};

class WeatherSystem {
public:
    void Init();
    void Shutdown();
    void Update(float dt, const Vec3& cameraPos);
    void Render(const CameraController& camera, const WeatherConfig& config);

private:
    struct WeatherParticle {
        Vec3 position;
        float phase;  // snow wobble phase
    };

    struct WeatherInstanceData {
        Vec3 position;   // 12 bytes
        float opacity;   //  4 bytes
    };  // 16 bytes

    std::vector<WeatherParticle> m_Particles;
    std::vector<WeatherInstanceData> m_Instances;

    // GPU resources
    std::shared_ptr<VertexArray> m_VAO;
    std::shared_ptr<VertexBuffer> m_QuadVBO;
    std::shared_ptr<VertexBuffer> m_InstanceVBO;
    std::shared_ptr<Shader> m_Shader;

    int m_ActiveCount = 0;
    int m_AllocatedMax = 0;
    float m_Time = 0.0f;
    float m_DeltaTime = 0.0f;
    Vec3 m_LastCameraPos{0.0f};
    bool m_Initialized = false;

    void EnsureCapacity(int maxParticles);
    void RespawnParticle(WeatherParticle& p, const Vec3& cameraPos,
                         const Vec3& areaSize, bool randomY);
};

} // namespace Puluo
