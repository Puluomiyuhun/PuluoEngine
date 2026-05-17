#version 450 core

const float PI = 3.14159265359;
const int MAX_LIGHTS = 16;

struct LightData {
    vec4 positionAndType;       // xyz = pos, w = type (0=dir,1=point,2=spot)
    vec4 directionAndIntensity; // xyz = dir, w = intensity
    vec4 color;                 // xyz = color
    vec4 attenuation;           // x = constant, y = linear, z = quadratic
    vec4 cutoff;                // x = innerCutoff, y = outerCutoff
};

layout(std140, binding = 0) uniform LightBuffer {
    LightData lights[MAX_LIGHTS];
    vec4 ambientAndCount;       // xyz = ambient, w = lightCount
};

// PBR material uniforms
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAOMap;

// Fallback values when textures are not available
uniform vec3  uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;

uniform bool uUseAlbedoMap;
uniform bool uUseNormalMap;
uniform bool uUseMetallicMap;
uniform bool uUseRoughnessMap;
uniform bool uUseAOMap;

uniform vec3 uCamPos;

// Fog uniforms
uniform bool  uFogEnabled;
uniform float uFogDensity;       // Base fog density
uniform float uFogHeightFalloff; // How quickly fog decreases with height
uniform float uFogMaxOpacity;    // Maximum fog opacity [0..1]
uniform vec3  uFogColor;         // Base inscattering color (ambient sky blue)
uniform float uFogStartDistance; // Distance at which fog begins
// Directional inscattering (sun through fog)
uniform vec3  uFogDirInscatterColor;  // Sun transmittance color
uniform float uFogDirInscatterExp;    // Cosine lobe exponent
uniform float uFogDirInscatterStartDist;
uniform vec3  uFogSunDirection;       // Direction TO the sun

// IBL maps
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D   uBrdfLUT;
uniform bool uUseIBL;
uniform float uIBLIntensity;

// Alpha mask
uniform sampler2D uMaskMap;
uniform bool  uUseAlphaMask;
uniform float uAlphaCutoff;

// Subsurface scattering
uniform bool  uUseSSS;
uniform vec3  uSSSColor;
uniform float uSSSStrength;

// CSM Shadow uniforms
uniform sampler2DArrayShadow uShadowMap; // slot 8
uniform mat4  uLightSpaceMatrices[4];
uniform float uCascadeSplits[4];
uniform int   uCascadeCount;
uniform float uShadowNormalBias;
uniform bool  uShadowEnabled;

// SSAO
uniform sampler2D uSSAOMap;
uniform bool  uSSAOEnabled;
uniform vec2  uScreenSize;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
} fs_in;

out vec4 FragColor;
layout(location = 1) out int oEntityID;

// Entity ID for mouse picking
uniform int uEntityID;

// ---- PBR Functions ----

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001);
}

// Geometry Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel with roughness (for IBL ambient specular)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ---- CSM Shadow Sampling ----

// Sample shadow for a specific cascade (no bias computation, caller provides biasedPos)
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

    // Select cascade based on distance from camera
    float dist = length(worldPos - uCamPos);
    int cascadeIndex = uCascadeCount - 1;
    for (int i = 0; i < uCascadeCount - 1; i++) {
        if (dist < uCascadeSplits[i]) {
            cascadeIndex = i;
            break;
        }
    }

    // Normal bias: push along normal to prevent acne
    vec3 biasedPos = worldPos + normal * uShadowNormalBias * (1.0 + float(cascadeIndex) * 0.5);

    float shadowCurrent = SampleShadowCascade(biasedPos, cascadeIndex);

    // Blend between cascades at boundary to eliminate hard seams
    // Use a transition zone of 20% of the cascade range
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

void main() {
    // Alpha mask discard (early out)
    if (uUseAlphaMask) {
        float mask = texture(uMaskMap, fs_in.TexCoord).r;
        if (mask < uAlphaCutoff) discard;
    }

    // Sample material properties
    vec3  albedo    = uUseAlbedoMap    ? pow(texture(uAlbedoMap, fs_in.TexCoord).rgb, vec3(2.2)) : uAlbedo;
    float metallic  = uUseMetallicMap  ? texture(uMetallicMap, fs_in.TexCoord).b  : uMetallic;
    float roughness = uUseRoughnessMap ? texture(uRoughnessMap, fs_in.TexCoord).g : uRoughness;
    float ao        = uUseAOMap        ? texture(uAOMap, fs_in.TexCoord).r        : uAO;

    // Normal
    vec3 N;
    if (uUseNormalMap) {
        N = texture(uNormalMap, fs_in.TexCoord).rgb * 2.0 - 1.0;
        N = normalize(fs_in.TBN * N);
    } else {
        N = normalize(fs_in.Normal);
    }

    // Geometric specular anti-aliasing (Tokuyoshi 2017)
    // Widens specular lobe where normal varies rapidly across pixels,
    // preventing sub-pixel highlight flickering in crevices and edges.
    {
        vec3 dNdx = dFdx(N);
        vec3 dNdy = dFdy(N);
        float variance = dot(dNdx, dNdx) + dot(dNdy, dNdy);
        float kernelRoughness = min(variance * 0.5, 0.18);
        roughness = sqrt(roughness * roughness + kernelRoughness);
    }

    vec3 V = normalize(uCamPos - fs_in.FragPos);

    // Base reflectivity
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Lighting accumulation
    int lightCount = int(ambientAndCount.w);
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightCount; i++) {
        vec3 lightPos   = lights[i].positionAndType.xyz;
        int  lightType  = int(lights[i].positionAndType.w);
        vec3 lightDir   = lights[i].directionAndIntensity.xyz;
        float intensity = lights[i].directionAndIntensity.w;
        vec3 lightColor = lights[i].color.rgb * intensity;

        vec3 L;
        float attenuation = 1.0;

        if (lightType == 0) {
            // Directional
            L = normalize(-lightDir);
        } else {
            // Point or Spot
            vec3 toLight = lightPos - fs_in.FragPos;
            float dist = length(toLight);
            L = normalize(toLight);

            float cst = lights[i].attenuation.x;
            float lin = lights[i].attenuation.y;
            float qua = lights[i].attenuation.z;
            attenuation = 1.0 / (cst + lin * dist + qua * dist * dist);

            if (lightType == 2) {
                // Spot
                float theta = dot(L, normalize(-lightDir));
                float inner = lights[i].cutoff.x;
                float outer = lights[i].cutoff.y;
                float epsilon = inner - outer;
                float spotIntensity = clamp((theta - outer) / epsilon, 0.0, 1.0);
                attenuation *= spotIntensity;
            }
        }

        vec3 H = normalize(V + L);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator  = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);

        // Apply shadow for the first directional light
        float shadow = 1.0;
        if (lightType == 0) {
            shadow = SampleShadowCSM(fs_in.FragPos, N);
        }

        Lo += (kD * albedo / PI + specular) * lightColor * attenuation * NdotL * shadow;

        // Subsurface scattering (wrap lighting for thin translucent surfaces)
        if (uUseSSS) {
            float NdotLBack = max(dot(-N, L), 0.0);
            float VdotL = max(dot(V, -L), 0.0);
            float sss = NdotLBack * 0.6 + VdotL * 0.4;
            vec3 sssContrib = albedo * uSSSColor * sss * uSSSStrength;
            Lo += sssContrib * lightColor * attenuation * shadow;
        }
    }

    // Ambient (IBL or flat)
    vec3 ambient;
    if (uUseIBL) {
        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        // Diffuse IBL
        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 diffuse = irradiance * albedo;

        // Specular IBL
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(uBrdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

        ambient = (kD * diffuse + specular) * ao * uIBLIntensity;
    } else {
        ambient = ambientAndCount.rgb * albedo * ao;
    }
    // Screen-space ambient occlusion
    if (uSSAOEnabled) {
        vec2 ssaoUV = gl_FragCoord.xy / uScreenSize;
        float ssao = texture(uSSAOMap, ssaoUV).r;
        ambient *= ssao;
    }
    vec3 color = ambient + Lo;

    // Exponential height fog (UE-style: base + directional inscattering)
    if (uFogEnabled) {
        vec3 rayDir = fs_in.FragPos - uCamPos;
        float dist = length(rayDir);
        vec3 rayDirNorm = rayDir / max(dist, 0.001);
        float effectiveDist = max(dist - uFogStartDistance, 0.0);

        // Height-based density: analytical integration
        float fragHeight = fs_in.FragPos.y;
        float camHeight  = uCamPos.y;
        float heightDiff = fragHeight - camHeight;

        float lineIntegralShared;
        if (abs(heightDiff) > 0.001) {
            float a = uFogHeightFalloff * heightDiff;
            lineIntegralShared = uFogDensity * exp(-uFogHeightFalloff * camHeight) * (1.0 - exp(-a)) / a;
        } else {
            lineIntegralShared = uFogDensity * exp(-uFogHeightFalloff * camHeight);
        }

        // Base fog factor (transmission)
        float baseFogIntegral = lineIntegralShared * effectiveDist;
        float expFogFactor = clamp(exp(-baseFogIntegral), 1.0 - uFogMaxOpacity, 1.0);

        // Base inscattering (ambient sky color)
        vec3 baseFog = uFogColor * (1.0 - expFogFactor);

        // Directional inscattering (sun lobe)
        float cosAngle = dot(rayDirNorm, uFogSunDirection);
        vec3 dirLightInscattering = uFogDirInscatterColor * pow(max(cosAngle, 0.0), uFogDirInscatterExp);
        float dirEffectiveDist = max(dist - uFogDirInscatterStartDist, 0.0);
        float dirFogIntegral = lineIntegralShared * dirEffectiveDist;
        float dirFogFactor = clamp(exp(-dirFogIntegral), 0.0, 1.0);
        vec3 directionalFog = dirLightInscattering * (1.0 - dirFogFactor);

        // Final: attenuate scene + add inscattered light
        color = color * expFogFactor + baseFog + directionalFog;
    }

    // HDR tonemapping (Reinhard) + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
    oEntityID = uEntityID;
}
