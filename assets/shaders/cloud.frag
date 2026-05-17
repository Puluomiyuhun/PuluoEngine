#version 450 core

const float PI = 3.14159265359;
const float EARTH_RADIUS = 6371000.0;

// Atmospheric scattering constants (match atmosphere.frag)
const float ATMO_RADIUS  = 6471000.0;
const float H_RAYLEIGH   = 8000.0;
const float H_MIE        = 1200.0;
const vec3  BETA_RAYLEIGH = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const float BETA_MIE      = 21e-6;

in vec3 vLocalPos;
out vec4 FragColor;

uniform vec3  uCamPos;

// Sun
uniform vec3  uSunDirection;
uniform float uSunIntensity;

// Cloud layer
uniform float uCloudBottom;
uniform float uCloudThickness;
uniform float uCoverage;
uniform float uCloudDensity;
uniform float uBaseScale;
uniform float uDetailScale;
uniform float uTime;
uniform float uWindSpeed;
uniform vec3  uWindDirection;

// Lighting
uniform float uPhaseG;
uniform float uPowderStrength;
uniform vec3  uAmbientColor;
uniform float uAmbientStrength;

// 3D noise textures
uniform sampler3D uBaseNoise;
uniform sampler3D uDetailNoise;

// ---- Utility ----

// Compute atmospheric extinction for sunlight reaching a given altitude
// Integrates optical depth along the sun's path through the atmosphere
vec3 AtmosphericSunColor(vec3 sunDir, float altitude, float intensity) {
    // Origin at the cloud altitude
    vec3 pos = vec3(0.0, EARTH_RADIUS + altitude, 0.0);

    // March toward the sun through the atmosphere
    float a = dot(sunDir, sunDir);
    float b = 2.0 * dot(sunDir, pos);
    float c = dot(pos, pos) - ATMO_RADIUS * ATMO_RADIUS;
    float disc = b * b - 4.0 * a * c;
    float tAtmo = (-b + sqrt(max(disc, 0.0))) / (2.0 * a);

    const int STEPS = 8;
    float segLen = tAtmo / float(STEPS);
    float odR = 0.0, odM = 0.0;
    for (int i = 0; i < STEPS; i++) {
        vec3 sp = pos + sunDir * (float(i) + 0.5) * segLen;
        float alt = length(sp) - EARTH_RADIUS;
        odR += exp(-alt / H_RAYLEIGH) * segLen;
        odM += exp(-alt / H_MIE) * segLen;
    }

    vec3 extinction = exp(-(BETA_RAYLEIGH * odR + BETA_MIE * 0.7 * odM));

    // Push sunset color from yellow toward orange/pink
    // At low sun angles, slightly suppress green channel
    float lowSun = 1.0 - smoothstep(0.0, 0.3, sunDir.y);
    extinction.g *= mix(1.0, 0.75, lowSun);

    return extinction * intensity;
}

// Approximate sky ambient color based on sun elevation
vec3 SkyAmbientColor(vec3 sunDir) {
    float sunY = clamp(sunDir.y, -0.1, 1.0);

    // Zenith color: blue at noon, deep blue-purple at sunset
    vec3 zenith = mix(vec3(0.25, 0.2, 0.4), vec3(0.35, 0.5, 0.75), smoothstep(0.0, 0.4, sunY));

    // Horizon color: warm orange at sunset, lighter blue at noon
    vec3 horizon = mix(vec3(0.6, 0.35, 0.2), vec3(0.5, 0.6, 0.7), smoothstep(0.0, 0.3, sunY));

    // Blend: clouds see a mix of zenith (top) and horizon (sides)
    vec3 ambient = mix(horizon, zenith, 0.5);

    // Fade to near-zero when sun is below horizon
    float nightFade = smoothstep(-0.1, 0.05, sunY);
    return ambient * nightFade;
}

vec2 RaySphere(vec3 origin, vec3 dir, float radius) {
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, origin);
    float c = dot(origin, origin) - radius * radius;
    float disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return vec2(-1.0);
    float sq = sqrt(disc);
    return vec2((-b - sq) / (2.0 * a), (-b + sq) / (2.0 * a));
}

float HG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

float remap(float v, float lo, float hi, float nlo, float nhi) {
    return nlo + (v - lo) * (nhi - nlo) / (hi - lo);
}

// Soft cumulus height gradient — rounder profile with soft bottom
float HeightGradient(float h) {
    return smoothstep(0.0, 0.35, h) * smoothstep(1.0, 0.65, h);
}

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// ---- Cloud density ----

float SampleCloudDensity(vec3 worldPos) {
    float altitude = length(worldPos) - EARTH_RADIUS;
    float hFrac = clamp((altitude - uCloudBottom) / uCloudThickness, 0.0, 1.0);

    vec3 surfacePos = worldPos - vec3(0.0, EARTH_RADIUS, 0.0);
    vec2 horizPos = surfacePos.xz;
    vec2 wind2D = uWindDirection.xz * uWindSpeed * uTime;
    vec2 uvBase = (horizPos + wind2D) * uBaseScale;

    // Low-freq UV distortion to break tiling
    vec3 uvLow = vec3(uvBase * 0.31, 0.5);
    vec4 lowSample = textureLod(uBaseNoise, uvLow, 2.0);
    vec2 distortion = (lowSample.rg - 0.5) * 0.4;

    // Base shape
    vec3 baseUV = vec3(uvBase + distortion, hFrac * 0.6);
    vec4 baseSample = textureLod(uBaseNoise, baseUV, 0.0);

    float perlinWorley = baseSample.r;
    float worleyFBM = baseSample.g * 0.625 + baseSample.b * 0.25 + baseSample.a * 0.125;

    float baseCloud = remap(perlinWorley, worleyFBM - 1.0, 1.0, 0.0, 1.0);
    baseCloud = clamp(baseCloud, 0.0, 1.0);

    // Height gradient
    baseCloud *= HeightGradient(hFrac);

    // Large-scale coverage variation
    float largeCoverage = lowSample.r * 0.6 + lowSample.g * 0.4;
    largeCoverage = smoothstep(0.25, 0.75, largeCoverage);

    // *** KEY FIX: Soft coverage instead of hard remap cutoff ***
    // Use smoothstep for gradual edge falloff instead of linear remap
    float threshold = 1.0 - uCoverage * mix(0.3, 1.0, largeCoverage);
    // Wide transition band: from threshold-0.2 to threshold+0.1
    baseCloud = smoothstep(threshold - 0.05, threshold + 0.25, baseCloud);

    // Detail erosion — aggressive at edges for wispy dissolution
    if (baseCloud > 0.01) {
        vec3 wind3D = uWindDirection * uWindSpeed * uTime;
        vec3 detailUV = surfacePos * uDetailScale + wind3D * uDetailScale * 0.5;
        vec4 detailSample = textureLod(uDetailNoise, detailUV, 0.0);
        float detailFBM = detailSample.r * 0.625 + detailSample.g * 0.25 + detailSample.b * 0.125;

        float detailMod = mix(detailFBM, 1.0 - detailFBM, clamp(hFrac * 2.0, 0.0, 1.0));

        // Stronger erosion at cloud edges (where baseCloud is small)
        float edgeFactor = 1.0 - smoothstep(0.0, 0.5, baseCloud);  // 1 at edge, 0 in core
        float erosion = detailMod * mix(0.2, 0.6, edgeFactor);
        baseCloud = clamp(baseCloud - erosion, 0.0, 1.0);
    }

    // Final soft curve for gentle density ramp
    baseCloud = baseCloud * baseCloud;  // quadratic = soft near 0, strong near 1

    return baseCloud * uCloudDensity;
}

// Light march with multi-scattering
vec2 LightMarch(vec3 pos) {
    const int LIGHT_STEPS = 6;
    float cloudTop = EARTH_RADIUS + uCloudBottom + uCloudThickness;

    vec2 tTop = RaySphere(pos, uSunDirection, cloudTop);
    float marchDist = min(max(tTop.y, 0.0), uCloudThickness * 1.5);
    float stepSz = marchDist / float(LIGHT_STEPS);

    float totalDensity = 0.0;
    for (int i = 0; i < LIGHT_STEPS; i++) {
        vec3 sp = pos + uSunDirection * (float(i) + 0.5) * stepSz;
        float alt = length(sp) - EARTH_RADIUS;
        if (alt < uCloudBottom || alt > uCloudBottom + uCloudThickness) continue;
        totalDensity += SampleCloudDensity(sp) * stepSz;
    }

    // Cap optical depth to prevent pitch-black bottoms
    totalDensity = min(totalDensity, 8.0);

    float beer = exp(-totalDensity);
    float powder = 1.0 - exp(-totalDensity * uPowderStrength);
    float single = beer * mix(1.0, powder, 0.35);

    // Multi-scattering (3 bounces) — strong to brighten cloud interior/bottom
    float multi = 0.0;
    float energy = 0.6;
    float ext = 0.4;
    for (int b = 0; b < 3; b++) {
        multi += energy * exp(-totalDensity * ext);
        energy *= 0.5;
        ext *= 0.25;
    }

    return vec2(single, multi);
}

void main() {
    vec3 viewDir = normalize(vLocalPos);

    if (viewDir.y < -0.01) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 origin = vec3(0.0, EARTH_RADIUS + 1.0, 0.0);

    float rBottom = EARTH_RADIUS + uCloudBottom;
    float rTop = EARTH_RADIUS + uCloudBottom + uCloudThickness;

    vec2 tBottom = RaySphere(origin, viewDir, rBottom);
    vec2 tTop = RaySphere(origin, viewDir, rTop);

    if (tTop.y < 0.0) {
        FragColor = vec4(0.0);
        return;
    }

    float tEnter = max(tBottom.y, 0.0);
    float tExit = tTop.y;

    if (tEnter >= tExit) {
        FragColor = vec4(0.0);
        return;
    }

    float marchLength = min(tExit - tEnter, uCloudThickness * 6.0);

    const int MAX_STEPS = 64;
    float stepSize = marchLength / float(MAX_STEPS);

    float dither = Hash(gl_FragCoord.xy + fract(uTime) * 100.0);
    float tStart = tEnter + dither * stepSize;

    float cosAngle = dot(viewDir, uSunDirection);
    float phaseVal = HG(cosAngle, uPhaseG) * 0.7 + HG(cosAngle, -0.3) * 0.3;
    float phaseIso = 1.0 / (4.0 * PI);

    // Physically-based sun color at cloud altitude (atmospheric extinction)
    float cloudMidAlt = uCloudBottom + uCloudThickness * 0.5;
    vec3 sunColor = AtmosphericSunColor(uSunDirection, cloudMidAlt, uSunIntensity);

    // Dynamic sky ambient
    vec3 skyAmbient = SkyAmbientColor(uSunDirection);

    vec3 totalLight = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < MAX_STEPS; i++) {
        if (transmittance < 0.01) break;

        float t = tStart + float(i) * stepSize;
        if (t > tExit) break;

        vec3 samplePos = origin + viewDir * t;

        float alt = length(samplePos) - EARTH_RADIUS;
        if (alt < uCloudBottom || alt > uCloudBottom + uCloudThickness) continue;

        float density = SampleCloudDensity(samplePos);
        if (density <= 0.0) continue;

        float mass = density * stepSize;

        vec2 lr = LightMarch(samplePos);
        float hFrac = (alt - uCloudBottom) / uCloudThickness;

        vec3 direct = sunColor * lr.x * phaseVal;
        vec3 multi = sunColor * lr.y * phaseIso;
        // Ground bounce light at bottom + sky ambient at top
        float ambientGrad = mix(0.7, 1.0, hFrac);  // bottom still gets 70%
        vec3 ambient = skyAmbient * uAmbientStrength * ambientGrad;
        // Extra ground bounce for bottom — tinted by sun for warm bottoms at sunset
        float nightFade = smoothstep(-0.1, 0.05, uSunDirection.y);
        vec3 bounceColor = mix(vec3(0.5, 0.35, 0.2), vec3(0.35, 0.3, 0.25), smoothstep(0.0, 0.3, uSunDirection.y));
        vec3 groundBounce = bounceColor * uAmbientStrength * smoothstep(0.4, 0.0, hFrac) * 0.5 * nightFade;

        totalLight += (direct + multi + ambient + groundBounce) * transmittance * mass;
        transmittance *= exp(-mass);
    }

    // Aerial perspective BEFORE tonemapping (in linear space)
    float distToCloud = tEnter;
    float aerialDist = max(distToCloud - 30000.0, 0.0);
    float aerialOpticalDepth = aerialDist * 1.5e-5;
    float aerialTrans = exp(-aerialOpticalDepth);

    // Sky inscattering color in linear space — bright at noon, warm at sunset
    vec3 skyInscatter = sunColor * 0.15 + skyAmbient * 0.6;
    totalLight = totalLight * aerialTrans + skyInscatter * (1.0 - aerialTrans);

    // Tone map — boost exposure so clouds are whiter
    totalLight *= 1.5;
    totalLight = totalLight / (totalLight + vec3(1.0));
    totalLight = pow(totalLight, vec3(1.0 / 2.2));

    float alpha = 1.0 - transmittance;

    // Far clouds: alpha reduced — sky shows through
    alpha *= aerialTrans;

    // Horizon fade (prevent hard line at y=0)
    float horizonFade = smoothstep(0.0, 0.03, viewDir.y);
    alpha *= horizonFade;

    // Pre-multiplied alpha
    FragColor = vec4(totalLight * alpha, alpha);
}
