#version 450 core

// Physically-based atmospheric scattering
// Based on "Precomputed Atmospheric Scattering" (Bruneton & Neyret) simplified for real-time

const float PI = 3.14159265359;

// Earth-like atmosphere constants
const float EARTH_RADIUS = 6371000.0;      // meters
const float ATMO_RADIUS  = 6471000.0;      // atmosphere top (100km above surface)
const float H_RAYLEIGH   = 8000.0;         // Rayleigh scale height (meters)
const float H_MIE        = 1200.0;          // Mie scale height (meters)

// Standard sea-level scattering coefficients
const vec3 BETA_RAYLEIGH = vec3(5.8e-6, 13.5e-6, 33.1e-6);   // per meter
const float BETA_MIE     = 21e-6;                              // per meter

in vec3 vLocalPos;
out vec4 FragColor;

uniform vec3  uSunDirection;     // Normalized direction TO the sun
uniform float uSunIntensity;     // Sun light intensity multiplier
uniform float uTurbidity;        // 1=clear, 10=hazy
uniform vec3  uRayleighCoeff;    // Custom Rayleigh coefficients (unused, using physical constants)
uniform float uMieCoeff;         // Custom Mie coefficient (unused)
uniform float uMieDirectionG;    // Mie anisotropy factor

// Sky light capture mode: output linear HDR (no tonemapping, gamma, fog, sun disk)
uniform bool  uLinearOutput;

// Fog uniforms (same as PBR shader)
uniform bool  uFogEnabled;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogMaxOpacity;
uniform vec3  uFogColor;
// Directional inscattering
uniform vec3  uFogDirInscatterColor;
uniform float uFogDirInscatterExp;
uniform vec3  uFogSunDirection;

// Rayleigh phase function
float PhaseRayleigh(float cosTheta) {
    return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

// Henyey-Greenstein phase function (Mie)
float PhaseMie(float cosTheta, float g) {
    float g2 = g * g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cosTheta * cosTheta);
    float denom = 8.0 * PI * (2.0 + g2) * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
    return num / denom;
}

// Ray-sphere intersection (returns distance to intersection, or -1)
// origin at (0, EARTH_RADIUS + altitude, 0)
float RaySphereIntersect(vec3 origin, vec3 dir, float radius) {
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, origin);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) return -1.0;
    return (-b + sqrt(discriminant)) / (2.0 * a);
}

void main() {
    vec3 viewDir = normalize(vLocalPos);

    // Camera position: on Earth surface
    vec3 origin = vec3(0.0, EARTH_RADIUS + 1.0, 0.0);

    // Ray march through atmosphere
    float tMax = RaySphereIntersect(origin, viewDir, ATMO_RADIUS);
    if (tMax < 0.0) tMax = 100000.0;

    // If ray hits ground, clip to ground intersection
    float tGround = -1.0;
    {
        float a = dot(viewDir, viewDir);
        float b = 2.0 * dot(viewDir, origin);
        float c = dot(origin, origin) - EARTH_RADIUS * EARTH_RADIUS;
        float disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            float t = (-b - sqrt(disc)) / (2.0 * a);
            if (t > 0.0) {
                tGround = t;
                tMax = min(tMax, t);
            }
        }
    }

    // Integration parameters
    const int NUM_SAMPLES = 16;
    const int NUM_SAMPLES_LIGHT = 8;
    float segmentLength = tMax / float(NUM_SAMPLES);

    // Scattering coefficients scaled by turbidity
    vec3 betaR = BETA_RAYLEIGH;
    float betaM = BETA_MIE * uTurbidity;

    // Accumulated optical depth along view ray
    float opticalDepthR = 0.0;
    float opticalDepthM = 0.0;

    // Accumulated inscattered light
    vec3 sumR = vec3(0.0);
    vec3 sumM = vec3(0.0);

    float cosTheta = dot(viewDir, uSunDirection);
    float phaseR = PhaseRayleigh(cosTheta);
    float phaseM = PhaseMie(cosTheta, uMieDirectionG);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Sample point along view ray
        float t = (float(i) + 0.5) * segmentLength;
        vec3 samplePos = origin + viewDir * t;
        float altitude = length(samplePos) - EARTH_RADIUS;

        // Density at sample point
        float densityR = exp(-altitude / H_RAYLEIGH) * segmentLength;
        float densityM = exp(-altitude / H_MIE) * segmentLength;

        opticalDepthR += densityR;
        opticalDepthM += densityM;

        // Light ray: march from sample point toward sun to compute optical depth
        float tSun = RaySphereIntersect(samplePos, uSunDirection, ATMO_RADIUS);
        if (tSun < 0.0) tSun = 100000.0;
        float segmentLengthLight = tSun / float(NUM_SAMPLES_LIGHT);

        float opticalDepthLightR = 0.0;
        float opticalDepthLightM = 0.0;
        bool occluded = false;

        for (int j = 0; j < NUM_SAMPLES_LIGHT; j++) {
            float tL = (float(j) + 0.5) * segmentLengthLight;
            vec3 samplePosLight = samplePos + uSunDirection * tL;
            float altitudeLight = length(samplePosLight) - EARTH_RADIUS;

            if (altitudeLight < 0.0) {
                occluded = true;
                break;
            }

            opticalDepthLightR += exp(-altitudeLight / H_RAYLEIGH) * segmentLengthLight;
            opticalDepthLightM += exp(-altitudeLight / H_MIE) * segmentLengthLight;
        }

        if (!occluded) {
            // Total extinction along view + light path
            vec3 tau = betaR * (opticalDepthR + opticalDepthLightR) +
                       betaM * 1.1 * (opticalDepthM + opticalDepthLightM);
            vec3 attenuation = exp(-tau);

            sumR += attenuation * densityR;
            sumM += attenuation * densityM;
        }
    }

    // Final color: inscattered sunlight
    vec3 skyColor = uSunIntensity * (sumR * betaR * phaseR + sumM * betaM * phaseM);

    // Linear output mode: skip sun disk, fog, tonemapping, gamma (for sky light capture)
    if (uLinearOutput) {
        // Ground color (dark horizon below)
        if (tGround > 0.0) {
            vec3 groundColor = vec3(0.04, 0.04, 0.03);
            vec3 groundExtinction = exp(-(betaR * opticalDepthR + betaM * 1.1 * opticalDepthM));
            skyColor = groundColor * groundExtinction * uSunIntensity * max(dot(uSunDirection, vec3(0, 1, 0)), 0.0) + skyColor * 0.1;
        }
        FragColor = vec4(skyColor, 1.0);
        return;
    }

    // Sun disk
    float sunCosAngle = 0.99985; // ~1 degree angular diameter
    float sunDisk = smoothstep(sunCosAngle - 0.0001, sunCosAngle + 0.0001, cosTheta);
    vec3 sunExtinction = exp(-(betaR * opticalDepthR + betaM * 1.1 * opticalDepthM));
    skyColor += sunDisk * uSunIntensity * 50.0 * sunExtinction;

    // Ground color (dark horizon below)
    if (tGround > 0.0) {
        vec3 groundColor = vec3(0.04, 0.04, 0.03);
        vec3 groundExtinction = exp(-(betaR * opticalDepthR + betaM * 1.1 * opticalDepthM));
        skyColor = groundColor * groundExtinction * uSunIntensity * max(dot(uSunDirection, vec3(0, 1, 0)), 0.0) + skyColor * 0.1;
    }

    // Height fog blending for sky (UE-style: base + directional inscattering)
    if (uFogEnabled) {
        float viewElevation = viewDir.y;

        // Fog strength based on view angle — strongest at horizon, fades looking up
        float horizonFog = exp(-max(viewElevation, 0.0) * 8.0 / max(uFogHeightFalloff, 0.001));
        if (viewElevation < 0.0) horizonFog = 1.0;
        float fogAmount = clamp(horizonFog * uFogMaxOpacity, 0.0, uFogMaxOpacity);

        // Base inscattering (ambient sky blue, in linear space)
        vec3 fogColorLinear = uFogColor * uFogColor; // approximate sRGB→linear
        // Desaturate fog slightly for softer look
        float fogLuma = dot(fogColorLinear, vec3(0.2126, 0.7152, 0.0722));
        fogColorLinear = mix(fogColorLinear, vec3(fogLuma), 0.5);
        // Shift slightly toward blue to counter purple tint
        fogColorLinear.b *= 1.2;
        fogColorLinear.r *= 0.8;
        vec3 baseFog = fogColorLinear * 2.5;

        // Directional inscattering (sun lobe toward sun direction)
        float cosAngle = dot(viewDir, uFogSunDirection);
        vec3 dirInscattering = uFogDirInscatterColor * uFogDirInscatterColor * 2.0; // sRGB→linear
        dirInscattering *= pow(max(cosAngle, 0.0), uFogDirInscatterExp);

        // Combine: base + directional
        vec3 totalFogColor = baseFog + dirInscattering;
        skyColor = mix(skyColor, totalFogColor, fogAmount);
    }

    // ACES filmic tonemapping (preserves saturation better than Reinhard)
    // Narkowicz 2015 ACES fit
    skyColor *= 0.6; // exposure adjustment
    skyColor = (skyColor * (2.51 * skyColor + 0.03)) / (skyColor * (2.43 * skyColor + 0.59) + 0.14);
    skyColor = clamp(skyColor, 0.0, 1.0);

    // Gamma correction
    skyColor = pow(skyColor, vec3(1.0 / 2.2));

    FragColor = vec4(skyColor, 1.0);
}
