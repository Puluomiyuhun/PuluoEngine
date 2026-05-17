#pragma once

#include "puluo/core/Math.h"
#include <vector>

namespace Puluo {

enum class LightType : int {
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct Light {
    LightType type = LightType::Directional;

    Vec3 position{0.0f, 5.0f, 0.0f};
    Vec3 direction{0.0f, -1.0f, 0.0f};
    Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    // Point / Spot attenuation
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;

    // Spot
    float innerCutoff = glm::cos(glm::radians(12.5f));
    float outerCutoff = glm::cos(glm::radians(17.5f));
};

// Aligned struct for UBO (std140 layout)
struct alignas(16) LightUBOData {
    // vec4 aligned fields
    Vec4 positionAndType;    // xyz = position, w = type
    Vec4 directionAndIntensity; // xyz = direction, w = intensity
    Vec4 color;              // xyz = color, w = unused
    Vec4 attenuation;        // x = constant, y = linear, z = quadratic, w = unused
    Vec4 cutoff;             // x = innerCutoff, y = outerCutoff, z/w = unused
};

static constexpr uint32_t MAX_LIGHTS = 16;

// Packed UBO data
struct LightBufferData {
    LightUBOData lights[MAX_LIGHTS];
    Vec4 ambientAndCount; // xyz = ambient color, w = light count (as float)
};

class LightManager {
public:
    void Clear();
    void AddLight(const Light& light);
    void SetAmbient(const Vec3& ambient) { m_Ambient = ambient; }

    const std::vector<Light>& GetLights() const { return m_Lights; }
    std::vector<Light>& GetLights() { return m_Lights; }
    uint32_t GetLightCount() const { return static_cast<uint32_t>(m_Lights.size()); }

    // Pack into UBO-ready struct
    LightBufferData GetBufferData() const;

private:
    std::vector<Light> m_Lights;
    Vec3 m_Ambient{0.03f, 0.03f, 0.03f};
};

} // namespace Puluo
