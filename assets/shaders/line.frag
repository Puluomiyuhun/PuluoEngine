#version 450 core

in vec3 vColor;
in vec3 vWorldPos;
out vec4 FragColor;

// Fog uniforms
uniform int   uFogEnabled;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogMaxOpacity;
uniform vec3  uFogColor;
uniform float uFogStartDistance;
uniform vec3  uCamPos;
// Directional inscattering
uniform vec3  uFogDirInscatterColor;
uniform float uFogDirInscatterExp;
uniform float uFogDirInscatterStartDist;
uniform vec3  uFogSunDirection;

void main() {
    vec3 color = vColor;

    if (uFogEnabled == 1) {
        vec3 rayDir = vWorldPos - uCamPos;
        float dist = length(rayDir);
        vec3 rayDirNorm = rayDir / max(dist, 0.001);
        float effectiveDist = max(dist - uFogStartDistance, 0.0);

        // Analytical height fog integration
        float heightDiff = vWorldPos.y - uCamPos.y;

        float lineIntegralShared;
        if (abs(heightDiff) > 0.001) {
            float a = uFogHeightFalloff * heightDiff;
            lineIntegralShared = uFogDensity * exp(-uFogHeightFalloff * uCamPos.y) * (1.0 - exp(-a)) / a;
        } else {
            lineIntegralShared = uFogDensity * exp(-uFogHeightFalloff * uCamPos.y);
        }

        // Base fog
        float baseFogIntegral = lineIntegralShared * effectiveDist;
        float expFogFactor = clamp(exp(-baseFogIntegral), 1.0 - uFogMaxOpacity, 1.0);
        vec3 baseFog = uFogColor * (1.0 - expFogFactor);

        // Directional inscattering
        float cosAngle = dot(rayDirNorm, uFogSunDirection);
        vec3 dirInscattering = uFogDirInscatterColor * pow(max(cosAngle, 0.0), uFogDirInscatterExp);
        float dirEffectiveDist = max(dist - uFogDirInscatterStartDist, 0.0);
        float dirFogIntegral = lineIntegralShared * dirEffectiveDist;
        float dirFogFactor = clamp(exp(-dirFogIntegral), 0.0, 1.0);
        vec3 directionalFog = dirInscattering * (1.0 - dirFogFactor);

        color = color * expFogFactor + baseFog + directionalFog;
    }

    FragColor = vec4(color, 1.0);
}
