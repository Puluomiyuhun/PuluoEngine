#pragma once

#include "puluo/core/Math.h"
#include <cmath>

namespace Puluo {

struct AtmosphereParams {
    Vec3 sunDirection{0.0f, 0.8f, 0.6f};  // Normalized direction TO the sun
    float sunIntensity = 22.0f;
    float turbidity = 1.5f;                // Atmospheric turbidity (1=clear, 10=hazy)
    Vec3 rayleighCoeff{5.5e-6f, 13.0e-6f, 22.4e-6f}; // Rayleigh scattering coefficients
    float mieCoeff = 21e-6f;               // Mie scattering coefficient
    float mieDirectionG = 0.93f;           // Mie preferred scattering direction (Henyey-Greenstein g)

    // Compute the sun's color after atmospheric extinction at current elevation.
    // Uses physically-based Rayleigh extinction: path = scaleHeight × airMass.
    // At noon: near white. At sunset: deep orange/red. At night: dark.
    Vec3 ComputeSunTransmittanceColor() const {
        float sunY = glm::clamp(sunDirection.y, -0.1f, 1.0f);

        // Air mass: 1 at zenith, ~38 at horizon (physical limit)
        float airMass = 1.0f / glm::max(sunY + 0.01f, 0.01f);
        airMass = glm::min(airMass, 40.0f);

        // Physical scale: Rayleigh scale height (8km) × air mass
        constexpr Vec3 BETA_RAYLEIGH{5.8e-6f, 13.5e-6f, 33.1e-6f};
        float scale = 8000.0f * airMass;

        Vec3 transmittance(
            std::exp(-BETA_RAYLEIGH.x * scale),
            std::exp(-BETA_RAYLEIGH.y * scale),
            std::exp(-BETA_RAYLEIGH.z * scale)
        );

        // Normalize so the brightest channel is 1.0
        float maxT = glm::max(glm::max(transmittance.x, transmittance.y), transmittance.z);
        if (maxT > 1e-6f) transmittance /= maxT;

        // Night darkening
        float nightFactor = glm::smoothstep(-0.1f, 0.15f, sunY);
        // Fade out directional inscattering as sun gets high (effect is mainly at low angles)
        float highSunFade = 1.0f - glm::smoothstep(0.15f, 0.45f, sunY);
        return transmittance * nightFactor * highSunFade;
    }

    // Compute a brightness factor for modulating base fog color by sun elevation.
    // Daylight: 1.0, dusk: fading, night: near 0.
    float ComputeSunBrightness() const {
        return glm::smoothstep(-0.1f, 0.3f, sunDirection.y);
    }
};

} // namespace Puluo
