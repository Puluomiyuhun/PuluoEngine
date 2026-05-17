#version 450 core

const float PI = 3.14159265359;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec2 vTexCoord;

out vec4 FragColor;

// Sun light
uniform vec3  uSunDirection;
uniform vec3  uSunColor;
uniform float uSunIntensity;

// Camera
uniform vec3 uCamPos;

// Global heightmap (same as tessellation, covers entire terrain 1:1)
uniform sampler2D uHeightmap;

// Terrain geometry info (for computing global UV)
uniform float uTerrainSize;
uniform vec3  uTerrainOrigin;

// Three-layer material textures
// Lower layer (height < threshold)
uniform sampler2D uLowerAlbedo;
uniform sampler2D uLowerNormal;
uniform sampler2D uLowerRoughness;
uniform bool uHasLowerAlbedo;
uniform bool uHasLowerNormal;
uniform bool uHasLowerRoughness;

// Upper layer (height >= threshold)
uniform sampler2D uUpperAlbedo;
uniform sampler2D uUpperNormal;
uniform sampler2D uUpperRoughness;
uniform bool uHasUpperAlbedo;
uniform bool uHasUpperNormal;
uniform bool uHasUpperRoughness;

// Slope layer (steep areas, highest priority)
uniform sampler2D uSlopeAlbedo;
uniform sampler2D uSlopeNormal;
uniform sampler2D uSlopeRoughness;
uniform bool uHasSlopeAlbedo;
uniform bool uHasSlopeNormal;
uniform bool uHasSlopeRoughness;

// Blend parameters
uniform float uHeightThreshold; // [0,1] split point for upper/lower
uniform float uSlopeThreshold;  // slope value above which slope material kicks in
uniform float uBlendSharpness;  // transition sharpness

// Per-layer normal strength (0 = flat, 1 = full)
uniform float uLowerNormalStrength;
uniform float uUpperNormalStrength;
uniform float uSlopeNormalStrength;

// Splat map
uniform sampler2D uSplatMap;
uniform bool uUseSplatMap;

uniform float uRoughness; // fallback roughness
uniform float uMetallic;

// Fog
uniform bool  uFogEnabled;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogMaxOpacity;
uniform vec3  uFogColor;
uniform float uFogStartDistance;
uniform vec3  uFogDirInscatterColor;
uniform float uFogDirInscatterExp;
uniform float uFogDirInscatterStartDist;
uniform vec3  uFogSunDirection;

// CSM Shadow uniforms
uniform sampler2DArrayShadow uShadowMap;
uniform mat4  uLightSpaceMatrices[4];
uniform float uCascadeSplits[4];
uniform int   uCascadeCount;
uniform float uShadowNormalBias;
uniform bool  uShadowEnabled;

// SSAO
uniform sampler2D uSSAOMap;
uniform bool  uSSAOEnabled;
uniform vec2  uScreenSize;

// IBL
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D   uBrdfLUT;
uniform bool  uUseIBL;
uniform float uIBLIntensity;

// Debug: 0=off, 1=shadow, 2=SSAO, 3=NdotL, 4=ambient, 5=sunColor, 6=layer blend
uniform int uDebugMode;

// Fresnel with roughness (for IBL)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Procedural fallback albedo for a layer
vec3 ProceduralLayerColor(float height01, float slopeVal) {
    vec3 grass = vec3(0.15, 0.28, 0.08);
    vec3 rock  = vec3(0.4, 0.38, 0.35);
    vec3 snow  = vec3(0.9, 0.9, 0.92);

    vec3 color = mix(grass, snow, smoothstep(0.4, 0.7, height01));
    color = mix(color, rock, smoothstep(0.3, 0.6, slopeVal));
    return color;
}

// ---- CSM Shadow Sampling ----

float SampleShadowCascade(vec3 biasedPos, int idx) {
    vec4 lsPos = uLightSpaceMatrices[idx] * vec4(biasedPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0).xy);
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            shadow += texture(uShadowMap, vec4(
                projCoords.xy + vec2(float(x), float(y)) * texelSize,
                float(idx),
                projCoords.z));
        }
    }
    shadow /= 25.0;
    return shadow;
}

float SampleShadowCSM(vec3 worldPos, vec3 normal) {
    if (!uShadowEnabled || uCascadeCount <= 0) return 1.0;

    float dist = length(worldPos - uCamPos);
    int cascadeIndex = uCascadeCount - 1;
    for (int i = 0; i < uCascadeCount - 1; i++) {
        if (dist < uCascadeSplits[i]) {
            cascadeIndex = i;
            break;
        }
    }

    vec3 biasedPos = worldPos + normal * uShadowNormalBias * (1.0 + float(cascadeIndex) * 0.5);

    float shadowCurrent = SampleShadowCascade(biasedPos, cascadeIndex);

    // Blend between cascades at boundary to eliminate hard seams
    if (cascadeIndex < uCascadeCount - 1) {
        float splitDist = uCascadeSplits[cascadeIndex];
        float prevSplit = (cascadeIndex > 0) ? uCascadeSplits[cascadeIndex - 1] : 0.0;
        float cascadeRange = splitDist - prevSplit;
        float transitionWidth = cascadeRange * 0.2;
        float fadeStart = splitDist - transitionWidth;

        if (dist > fadeStart) {
            float t = smoothstep(fadeStart, splitDist, dist);
            int nextIdx = cascadeIndex + 1;
            vec3 biasedPosNext = worldPos + normal * uShadowNormalBias * (1.0 + float(nextIdx) * 0.5);
            float shadowNext = SampleShadowCascade(biasedPosNext, nextIdx);
            shadowCurrent = mix(shadowCurrent, shadowNext, t);
        }
    }

    // Fade out shadows at the edge of the last cascade to avoid hard cutoff line
    float maxShadowDist = uCascadeSplits[uCascadeCount - 1];
    float fadeOutStart = maxShadowDist * 0.8;
    if (dist > fadeOutStart) {
        float fadeOut = smoothstep(fadeOutStart, maxShadowDist, dist);
        shadowCurrent = mix(shadowCurrent, 1.0, fadeOut);
    }

    return shadowCurrent;
}

// Sample a layer's PBR properties
struct LayerPBR {
    vec3  albedo;
    vec3  normal;
    float roughness;
};

LayerPBR SampleLayer(vec2 tc,
                     sampler2D albedoTex, bool hasAlbedo,
                     sampler2D normalTex, bool hasNormal,
                     sampler2D roughnessTex, bool hasRoughness,
                     vec3 fallbackAlbedo, float normalStrength) {
    LayerPBR layer;

    if (hasAlbedo) {
        layer.albedo = pow(texture(albedoTex, tc).rgb, vec3(2.2));
    } else {
        layer.albedo = fallbackAlbedo;
    }

    if (hasNormal) {
        vec3 n = texture(normalTex, tc).rgb * 2.0 - 1.0;
        layer.normal = vec3(n.xy * normalStrength, n.z);
    } else {
        layer.normal = vec3(0.0, 0.0, 1.0); // tangent-space up
    }

    if (hasRoughness) {
        layer.roughness = texture(roughnessTex, tc).r;
    } else {
        layer.roughness = uRoughness;
    }

    return layer;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangent);
    vec3 B = normalize(vBitangent);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(uSunDirection);

    mat3 TBN = mat3(T, B, N);

    // Tiled UV for PBR material sampling
    vec2 texCoord = vTexCoord;

    // Global UV for heightmap sampling (covers entire terrain 1:1)
    vec2 globalUV = (vWorldPos.xz - uTerrainOrigin.xz) / uTerrainSize;
    float height01 = texture(uHeightmap, globalUV).r; // [0,1]

    // Slope from geometry normal (before normal mapping)
    float slopeVal = 1.0 - N.y; // 0 = flat, 1 = vertical

    // Compute blend weights
    float wLower, wUpper, wSlope;

    if (uUseSplatMap) {
        // Sample splat map: R=Lower, G=Upper, B=Slope
        vec3 splatWeights = texture(uSplatMap, globalUV).rgb;
        wLower = splatWeights.r;
        wUpper = splatWeights.g;
        wSlope = splatWeights.b;
        // Normalize in case of precision issues
        float wSum = wLower + wUpper + wSlope;
        if (wSum > 0.0) {
            wLower /= wSum;
            wUpper /= wSum;
            wSlope /= wSum;
        }
    } else {
        // Procedural blend: height + slope
        float halfSharp = uBlendSharpness * 0.5;
        float heightBlend = smoothstep(uHeightThreshold - 1.0 / halfSharp,
                                       uHeightThreshold + 1.0 / halfSharp,
                                       height01);
        float slopeBlend = smoothstep(uSlopeThreshold - 1.0 / halfSharp,
                                      uSlopeThreshold + 1.0 / halfSharp,
                                      slopeVal);
        wLower = (1.0 - heightBlend) * (1.0 - slopeBlend);
        wUpper = heightBlend * (1.0 - slopeBlend);
        wSlope = slopeBlend;
    }

    // Sample three layers
    vec3 fallbackLower = vec3(0.15, 0.28, 0.08); // grass-ish
    vec3 fallbackUpper = vec3(0.9, 0.9, 0.92);   // snow-ish
    vec3 fallbackSlope = vec3(0.4, 0.38, 0.35);   // rock-ish

    LayerPBR lower = SampleLayer(texCoord,
        uLowerAlbedo, uHasLowerAlbedo,
        uLowerNormal, uHasLowerNormal,
        uLowerRoughness, uHasLowerRoughness,
        fallbackLower, uLowerNormalStrength);

    LayerPBR upper = SampleLayer(texCoord,
        uUpperAlbedo, uHasUpperAlbedo,
        uUpperNormal, uHasUpperNormal,
        uUpperRoughness, uHasUpperRoughness,
        fallbackUpper, uUpperNormalStrength);

    LayerPBR slopeLayer = SampleLayer(texCoord,
        uSlopeAlbedo, uHasSlopeAlbedo,
        uSlopeNormal, uHasSlopeNormal,
        uSlopeRoughness, uHasSlopeRoughness,
        fallbackSlope, uSlopeNormalStrength);

    // Blend: three-way weighted blend using computed weights
    vec3 albedo = lower.albedo * wLower + upper.albedo * wUpper + slopeLayer.albedo * wSlope;
    vec3 normalTS = lower.normal * wLower + upper.normal * wUpper + slopeLayer.normal * wSlope;
    float roughness = lower.roughness * wLower + upper.roughness * wUpper + slopeLayer.roughness * wSlope;

    // Apply normal map
    N = normalize(TBN * normalTS);

    // Geometric specular anti-aliasing
    {
        vec3 dNdx = dFdx(N);
        vec3 dNdy = dFdy(N);
        float variance = dot(dNdx, dNdx) + dot(dNdy, dNdy);
        float kernelRoughness = min(variance * 0.5, 0.18);
        roughness = sqrt(roughness * roughness + kernelRoughness);
    }

    // PBR lighting (GGX specular + Lambert diffuse)
    float NdotL = max(dot(N, L), 0.0);
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotV = max(dot(N, V), 0.001);

    // GGX distribution
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * denom * denom);

    // Schlick-GGX geometry
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));

    // Fresnel (Schlick, non-metallic terrain)
    float metallic = uMetallic;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    vec3 sunLight = uSunColor * uSunIntensity;
    float shadow = SampleShadowCSM(vWorldPos, N);
    vec3 Lo = (kD * albedo / PI + specular) * sunLight * NdotL * shadow;

    // Ambient (IBL or fixed fallback)
    vec3 ambient;
    if (uUseIBL) {
        vec3 F_ibl = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
        vec3 kS_ibl = F_ibl;
        vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

        // Diffuse IBL
        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 diffuseIBL = irradiance * albedo;

        // Specular IBL
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(uBrdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specularIBL = prefilteredColor * (F_ibl * brdf.x + brdf.y);

        ambient = (kD_ibl * diffuseIBL + specularIBL) * uIBLIntensity;
    } else {
        float sunY = uSunDirection.y;
        float nightFade = smoothstep(-0.1, 0.15, sunY);
        vec3 skyAmbient = mix(vec3(0.005), vec3(0.08, 0.1, 0.15), nightFade);
        ambient = albedo * skyAmbient;
    }
    // Screen-space ambient occlusion
    if (uSSAOEnabled) {
        vec2 ssaoUV = gl_FragCoord.xy / uScreenSize;
        float ssao = texture(uSSAOMap, ssaoUV).r;
        ambient *= ssao;
    }
    vec3 color = Lo + ambient;

    // Fog (same as PBR shader)
    if (uFogEnabled) {
        vec3 rayDir = vWorldPos - uCamPos;
        float dist = length(rayDir);
        vec3 rayDirNorm = rayDir / dist;

        float effectiveDist = max(dist - uFogStartDistance, 0.0);
        float camHeight = uCamPos.y;
        float heightDiff = rayDir.y;

        float lineIntegralShared;
        if (abs(heightDiff) > 0.01) {
            float aa = uFogHeightFalloff * heightDiff;
            lineIntegralShared = uFogDensity * exp(-uFogHeightFalloff * camHeight)
                               * (1.0 - exp(-aa)) / aa;
        } else {
            lineIntegralShared = uFogDensity * exp(-uFogHeightFalloff * camHeight);
        }

        float baseFogIntegral = lineIntegralShared * effectiveDist;
        float expFogFactor = clamp(exp(-baseFogIntegral), 1.0 - uFogMaxOpacity, 1.0);
        vec3 baseFog = uFogColor * (1.0 - expFogFactor);

        float cosAngle = dot(rayDirNorm, uFogSunDirection);
        vec3 dirInscatter = uFogDirInscatterColor * pow(max(cosAngle, 0.0), uFogDirInscatterExp);
        float dirDist = max(dist - uFogDirInscatterStartDist, 0.0);
        float dirFog = 1.0 - exp(-lineIntegralShared * dirDist);
        vec3 dirFogColor = dirInscatter * dirFog;

        color = color * expFogFactor + baseFog + dirFogColor;
    }

    // Debug visualization
    if (uDebugMode == 1) { FragColor = vec4(vec3(shadow), 1.0); return; }
    if (uDebugMode == 2) {
        float ao = 1.0;
        if (uSSAOEnabled) ao = texture(uSSAOMap, gl_FragCoord.xy / uScreenSize).r;
        FragColor = vec4(vec3(ao), 1.0); return;
    }
    if (uDebugMode == 3) { FragColor = vec4(vec3(NdotL), 1.0); return; }
    if (uDebugMode == 4) { FragColor = vec4(ambient * 10.0, 1.0); return; }
    if (uDebugMode == 5) {
        vec3 sc = uSunColor * uSunIntensity * 0.1;
        FragColor = vec4(sc, 1.0); return;
    }
    if (uDebugMode == 6) {
        // Visualize layer blend: R=lower, G=upper, B=slope
        FragColor = vec4(wLower, wUpper, wSlope, 1.0); return;
    }

    // Tonemap (ACES) + gamma
    color *= 0.6;
    color = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
    color = clamp(color, 0.0, 1.0);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
