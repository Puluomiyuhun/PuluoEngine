#pragma once

#include "puluo/core/Math.h"

namespace Puluo {

struct FogParams {
    bool enabled = false;
    float density = 0.02f;
    float heightFalloff = 0.2f;
    float maxOpacity = 1.0f;
    Vec3 fogColor{0.18f, 0.28f, 0.48f};  // Base inscattering color (ambient sky blue)
    float startDistance = 10.0f;

    // Directional inscattering (approximates sun light scattering through fog)
    Vec3 directionalInscatteringColor{1.0f, 0.85f, 0.5f};  // Auto-derived from atmosphere
    float directionalInscatteringExponent = 4.0f;           // Cosine lobe tightness
    float directionalInscatteringStartDistance = 20.0f;
    Vec3 sunDirection{0.0f, 0.8f, 0.6f};                    // Synced from atmosphere
};

} // namespace Puluo
