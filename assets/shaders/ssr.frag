#version 450 core

// SSR — Screen-Space Reflections via screen-space ray march + binary refinement
// Uses homogeneous-coordinate interpolation so each step is ~1 pixel on screen,
// eliminating the banding/staircase artefacts of view-space fixed-step marching.

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uDepthTexture;   // slot 0 — depth prepass (reversed-Z)
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
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = uInvProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Return the raw depth buffer value at a UV
float SampleDepth(vec2 uv) {
    return texture(uDepthTexture, uv).r;
}

// Linearize a reversed-Z depth value to view-space Z (negative in OpenGL)
// For reversed-Z infinite far: depth = near / (-z)
//   => -z = near / depth  => z = -near / depth
float LinearizeDepth(float d) {
    // uProjection[3][2] == near for reversed-Z infinite-far projection
    float near = uProjection[3][2];
    return -near / max(d, 1e-7);
}

void main() {
    float rawDepth = SampleDepth(vTexCoord);

    // Skip far plane (reversed-Z: sky = 0.0)
    if (rawDepth <= 0.0) {
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

    // ---- Screen-space ray march ----
    // Project start and a point along the ray into screen (pixel) coordinates.
    // Then step uniformly in screen space, interpolating depth via 1/w.

    vec3 rayEnd = viewPos + reflDir * uMaxDistance;

    // Project both endpoints to clip space
    vec4 clipStart = uProjection * vec4(viewPos, 1.0);
    vec4 clipEnd   = uProjection * vec4(rayEnd, 1.0);

    // Screen-space positions (pixels)
    vec2 ssStart = (clipStart.xy / clipStart.w * 0.5 + 0.5) * uScreenSize;
    vec2 ssEnd   = (clipEnd.xy   / clipEnd.w   * 0.5 + 0.5) * uScreenSize;

    // Handle degenerate case where ray end is behind camera
    if (clipEnd.w < 0.0) {
        // Clip ray to near plane: find t where w interpolation crosses 0
        float t = clipStart.w / (clipStart.w - clipEnd.w);
        t = clamp(t - 0.01, 0.0, 1.0);
        clipEnd = mix(clipStart, clipEnd, t);
        ssEnd = (clipEnd.xy / clipEnd.w * 0.5 + 0.5) * uScreenSize;
    }

    vec2 ssDelta = ssEnd - ssStart;
    float ssLength = max(abs(ssDelta.x), abs(ssDelta.y));

    // Determine how many steps: ~1 pixel per step, capped by uMaxSteps
    int numSteps = clamp(int(ssLength), 1, uMaxSteps);
    float invSteps = 1.0 / float(numSteps);

    // 1/w values for homogeneous interpolation (perspective-correct)
    float invW0 = 1.0 / clipStart.w;
    float invW1 = 1.0 / clipEnd.w;

    // March
    vec2 hitUV = vec2(0.0);
    bool hit = false;
    float marchT = 0.0;  // parametric position along ray at hit

    // Precompute per-step deltas
    vec2 ssStep = ssDelta * invSteps;
    float invWStep = (invW1 - invW0) * invSteps;

    vec2 ssCur = ssStart;
    float invWCur = invW0;

    for (int i = 1; i <= numSteps; i++) {
        ssCur += ssStep;
        invWCur += invWStep;

        vec2 uv = ssCur / uScreenSize;

        // Out of screen bounds
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            break;

        // Perspective-correct interpolation of view-space Z
        float t = float(i) * invSteps;
        float rayZ = 1.0 / invWCur;  // w at this point = view-space -z (for perspective)
        rayZ = -rayZ;  // view-space z is negative

        // Sample scene depth and convert to linear view-space Z
        float sceneDepthRaw = SampleDepth(uv);
        if (sceneDepthRaw <= 0.0) continue;  // sky
        float sceneZ = LinearizeDepth(sceneDepthRaw);

        // Check intersection: ray is behind surface (more negative z)
        float depthDiff = rayZ - sceneZ;

        if (depthDiff < 0.0 && depthDiff > -uThickness) {
            hitUV = uv;
            hit = true;
            marchT = t;

            // Binary search refinement in parametric space
            float lo = t - invSteps;
            float hi = t;

            for (int j = 0; j < uBinarySearchSteps; j++) {
                float mid = (lo + hi) * 0.5;

                // Interpolate screen position and 1/w
                float midInvW = mix(invW0, invW1, mid);
                vec2 midSS = mix(ssStart, ssEnd, mid);
                vec2 midUV = midSS / uScreenSize;

                float midRayZ = -1.0 / midInvW;
                float midSceneRaw = SampleDepth(midUV);
                float midSceneZ = LinearizeDepth(midSceneRaw);
                float midDiff = midRayZ - midSceneZ;

                if (midDiff < 0.0 && midDiff > -uThickness) {
                    hi = mid;
                    hitUV = midUV;
                } else {
                    lo = mid;
                }
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
    float distanceFade = 1.0 - clamp(marchT, 0.0, 1.0);

    float confidence = screenFade * facingFade * distanceFade;

    FragColor = vec4(reflectedColor, confidence);
}
