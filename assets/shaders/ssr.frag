#version 450 core

// SSR — Screen Space Reflections via linear ray march + binary search refinement

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uDepthTexture;   // slot 0 — depth prepass
uniform sampler2D uSceneColor;     // slot 1 — scene color

uniform mat4 uProjection;
uniform mat4 uInvProjection;
uniform mat4 uView;
uniform mat4 uInvView;
uniform vec2 uScreenSize;

uniform int   uMaxSteps;
uniform float uMaxDistance;
uniform float uThickness;
uniform float uFadeEdge;
uniform int   uBinarySearchSteps;

// Reconstruct view-space position from depth at given UV
vec3 ViewPosFromDepth(vec2 uv) {
    float depth = texture(uDepthTexture, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Project view-space position to screen UV
vec3 ProjectToScreen(vec3 viewPos) {
    vec4 clipPos = uProjection * vec4(viewPos, 1.0);
    clipPos.xy /= clipPos.w;
    vec2 uv = clipPos.xy * 0.5 + 0.5;
    return vec3(uv, clipPos.w);
}

void main() {
    float rawDepth = texture(uDepthTexture, vTexCoord).r;

    // Skip far plane (sky)
    if (rawDepth >= 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 viewPos = ViewPosFromDepth(vTexCoord);

    // Reconstruct view-space normal from depth cross-derivatives
    vec3 dPdx = dFdx(viewPos);
    vec3 dPdy = dFdy(viewPos);
    vec3 normal = normalize(cross(dPdx, dPdy));

    // Compute reflection direction in view space
    vec3 viewDir = normalize(viewPos);
    vec3 reflDir = reflect(viewDir, normal);

    // Skip reflections pointing towards camera (they won't find a hit)
    if (reflDir.z > 0.0) {
        FragColor = vec4(0.0);
        return;
    }

    // Linear ray march
    float stepSize = uMaxDistance / float(uMaxSteps);
    vec3 rayPos = viewPos;
    vec3 rayStep = reflDir * stepSize;

    vec2 hitUV = vec2(0.0);
    bool hit = false;

    for (int i = 0; i < uMaxSteps; i++) {
        rayPos += rayStep;

        vec3 projected = ProjectToScreen(rayPos);
        vec2 sampleUV = projected.xy;

        // Out of screen bounds
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            break;

        // Behind camera
        if (projected.z < 0.0)
            break;

        float sampledDepth = ViewPosFromDepth(sampleUV).z;
        float depthDiff = rayPos.z - sampledDepth;

        // Hit: ray is behind the surface but not too far behind
        if (depthDiff > 0.0 && depthDiff < uThickness) {
            hitUV = sampleUV;
            hit = true;

            // Binary search refinement
            vec3 binaryStep = rayStep * 0.5;
            vec3 binaryPos = rayPos;
            for (int j = 0; j < uBinarySearchSteps; j++) {
                binaryPos -= binaryStep;
                binaryStep *= 0.5;

                vec3 bp = ProjectToScreen(binaryPos);
                float bd = ViewPosFromDepth(bp.xy).z;
                float bDiff = binaryPos.z - bd;

                if (bDiff > 0.0) {
                    binaryPos -= binaryStep;
                } else {
                    binaryPos += binaryStep;
                }
                hitUV = bp.xy;
            }
            break;
        }
    }

    if (!hit) {
        FragColor = vec4(0.0);
        return;
    }

    // Sample reflected color
    vec3 reflectedColor = texture(uSceneColor, hitUV).rgb;

    // Fade near screen edges
    vec2 edgeFade = smoothstep(vec2(0.0), vec2(uFadeEdge), hitUV)
                  * (1.0 - smoothstep(vec2(1.0 - uFadeEdge), vec2(1.0), hitUV));
    float screenFade = edgeFade.x * edgeFade.y;

    // Fade based on facing direction (glancing angles produce better results)
    float facingFade = 1.0 - max(dot(viewDir, reflDir), 0.0);

    // Fade based on ray travel distance
    float rayLength = length(rayPos - viewPos);
    float distanceFade = 1.0 - clamp(rayLength / uMaxDistance, 0.0, 1.0);

    float confidence = screenFade * facingFade * distanceFade;

    FragColor = vec4(reflectedColor, confidence);
}
