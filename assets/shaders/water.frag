#version 450 core

in vec3 vWorldPos;
in vec2 vTexCoord;
in vec4 vClipPos;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out int oEntityID;

uniform sampler2D uSceneColor;     // slot 10 — copy of scene color before water
uniform sampler2D uDepthTexture;   // slot 11 — depth prepass texture

uniform vec3 uCamPos;
uniform float uTime;
uniform vec2 uScreenSize;

// Water params
uniform vec3  uWaterTint;
uniform float uWaveSpeed;
uniform float uWaveScale;
uniform float uReflectionStrength;
uniform float uOpacity;
uniform float uFresnelPower;
uniform int   uEntityID;

uniform mat4 uProjection;

float LinearizeDepth(float d) {
    // Reversed-Z infinite far plane: d = near / (-z_eye)
    // → z_eye = -near / d  (where near = uProjection[3][2])
    if (d <= 0.0) return -10000.0; // far plane
    return -uProjection[3][2] / d;
}

void main() {
    // Screen-space UV from clip position
    vec2 screenUV = (vClipPos.xy / vClipPos.w) * 0.5 + 0.5;

    // Multi-octave wave distortion
    float t = uTime * uWaveSpeed;
    vec2 wave1 = sin(vWorldPos.xz * uWaveScale + t * vec2(1.0, 0.7));
    vec2 wave2 = sin(vWorldPos.xz * uWaveScale * 2.37 + t * vec2(0.6, 1.1) + 2.5);
    vec2 wave3 = sin(vWorldPos.xz * uWaveScale * 0.53 + t * vec2(0.3, 0.5) + 5.0);
    vec2 distortion = (wave1 + wave2 * 0.5 + wave3 * 0.25) * 0.015;

    // --- Refraction: distorted screen sampling (scene beneath the water) ---
    vec2 refractUV = screenUV + distortion;
    refractUV = clamp(refractUV, vec2(0.001), vec2(0.999));
    vec3 sceneColor = texture(uSceneColor, refractUV).rgb;

    // View direction and wave normal
    vec3 viewDir = normalize(uCamPos - vWorldPos);
    vec3 waterNormal = normalize(vec3(distortion.x * 2.0, 1.0, distortion.y * 2.0));

    // Fresnel: looking straight down → see scene below, grazing → water tint
    float NdotV = max(dot(vec3(0.0, 1.0, 0.0), viewDir), 0.0);
    float fresnel = pow(1.0 - NdotV, uFresnelPower);
    fresnel = clamp(fresnel, 0.0, 1.0);

    // Depth-based shore fade
    float sceneDepth = abs(LinearizeDepth(texture(uDepthTexture, screenUV).r));
    float waterDepth = abs(LinearizeDepth(gl_FragCoord.z));
    float depthDiff = sceneDepth - waterDepth;
    float shoreFade = (depthDiff < 0.0) ? 1.0 : clamp(depthDiff * 2.0, 0.0, 1.0);

    // Depth-based tint: deeper water → more tinted, shallow → more transparent
    float depthTint = clamp(depthDiff * 0.3, 0.0, 1.0);

    // Water surface color: blend scene color with water tint
    // - fresnel drives angle-based tinting (grazing = more tint)
    // - uReflectionStrength controls overall tint/opacity of the water surface
    // - depthTint makes shallow areas more transparent
    float tintAmount = max(fresnel, uReflectionStrength) * depthTint;
    vec3 waterColor = mix(sceneColor, uWaterTint, tintAmount);

    // Specular highlight from wave normals
    vec3 sunDir = normalize(vec3(0.5, 0.8, 0.3));
    vec3 halfVec = normalize(viewDir + sunDir);
    float spec = pow(max(dot(waterNormal, halfVec), 0.0), 64.0) * 0.5;
    waterColor += vec3(spec);

    float alpha = uOpacity * shoreFade;

    FragColor = vec4(waterColor, alpha);
    oEntityID = uEntityID;
}
