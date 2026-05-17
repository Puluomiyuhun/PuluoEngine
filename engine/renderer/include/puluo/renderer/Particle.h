#pragma once

#include "puluo/core/Math.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Puluo {

enum class ParticleBlendMode : uint8_t {
    Alpha    = 0,   // Standard alpha blending (smoke, dust)
    Additive = 1    // Additive blending (fire, sparks, glow)
};

// Per-emitter configuration (POD, like CloudParams/FogParams)
struct ParticleEmitterParams {
    bool enabled = true;

    // Emission
    float emissionRate = 50.0f;      // particles per second
    int maxParticles   = 1000;

    // Initial velocity range (random between min and max)
    Vec3 velocityMin{-0.5f, 1.0f, -0.5f};
    Vec3 velocityMax{ 0.5f, 3.0f,  0.5f};

    // Lifetime range (seconds)
    float lifetimeMin = 1.0f;
    float lifetimeMax = 3.0f;

    // Size over life (lerp from start to end)
    float sizeStart = 0.2f;
    float sizeEnd   = 0.05f;

    // Color over life (lerp from start to end, RGBA)
    Vec4 colorStart{1.0f, 0.8f, 0.3f, 1.0f};
    Vec4 colorEnd{0.5f, 0.1f, 0.0f, 0.0f};

    // Physics
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    float drag = 0.5f;

    // Rendering
    ParticleBlendMode blendMode = ParticleBlendMode::Additive;

    // Preset name (empty for custom)
    std::string presetName;
};

// Named presets
ParticleEmitterParams GetParticlePreset(const std::string& name);
std::vector<std::string> GetParticlePresetNames();

} // namespace Puluo
