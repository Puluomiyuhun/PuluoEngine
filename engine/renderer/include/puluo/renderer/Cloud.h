#pragma once

#include "puluo/core/Math.h"

namespace Puluo {

struct CloudParams {
    bool enabled = false;
    float cloudLayerBottom = 4000.0f;    // meters above ground
    float cloudLayerThickness = 4500.0f;  // cloud layer height range
    float coverage = 0.5f;               // 0=clear, 1=overcast
    float density = 0.001f;              // absorption coefficient (per meter)
    float detailScale = 0.001f;         // detail noise UV scale
    float baseScale = 0.00008f;         // base noise UV scale (smaller = larger clouds)
    float windSpeed = 5.0f;             // horizontal wind (m/s)
    Vec3 windDirection{1.0f, 0.0f, 0.0f};
    float phaseG = 0.35f;              // HG phase function asymmetry (forward scattering)
    float powderStrength = 2.0f;       // powder/sugar effect intensity
    Vec3 ambientColor{0.5f, 0.6f, 0.75f}; // sky ambient for cloud bottom
    float ambientStrength = 0.15f;
};

} // namespace Puluo
