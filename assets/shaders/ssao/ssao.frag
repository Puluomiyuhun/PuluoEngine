#version 450 core

out float FragColor;

in vec2 vTexCoord;

uniform sampler2D uDepthTexture;
uniform sampler2D uNoiseTexture;   // kept for compatibility, not sampled

uniform vec3  uSamples[64];
uniform mat4  uProjection;
uniform mat4  uInvProjection;
uniform vec2  uNoiseScale;         // (width/4, height/4) — used for pixel coord
uniform int   uKernelSize;
uniform float uRadius;
uniform float uBias;
uniform float uPower;

// Reconstruct view-space position from depth at given UV
vec3 ViewPosFromDepth(vec2 uv) {
    float depth = texture(uDepthTexture, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Stable normal reconstruction using min-difference neighbor pairs
// Picks the derivative direction (forward vs backward) with smaller depth delta
vec3 ReconstructNormal(vec3 fragPos, vec2 uv) {
    vec2 texelSize = 1.0 / vec2(textureSize(uDepthTexture, 0));

    vec3 posL = ViewPosFromDepth(uv - vec2(texelSize.x, 0.0));
    vec3 posR = ViewPosFromDepth(uv + vec2(texelSize.x, 0.0));
    vec3 posD = ViewPosFromDepth(uv - vec2(0.0, texelSize.y));
    vec3 posU = ViewPosFromDepth(uv + vec2(0.0, texelSize.y));

    vec3 dxLeft  = fragPos - posL;
    vec3 dxRight = posR - fragPos;
    vec3 dyDown  = fragPos - posD;
    vec3 dyUp    = posU - fragPos;

    // Pick the pair with smaller depth change (more reliable at edges)
    vec3 dx = (abs(dxLeft.z) < abs(dxRight.z)) ? dxLeft : dxRight;
    vec3 dy = (abs(dyDown.z) < abs(dyUp.z))    ? dyDown : dyUp;

    vec3 normal = normalize(cross(dy, dx));
    // Ensure normal points toward camera (view-space Z is negative)
    if (normal.z < 0.0) normal = -normal;
    return normal;
}

// Interleaved gradient noise — deterministic per-pixel, no tiling artifacts
float InterleavedGradientNoise(vec2 pixelCoord) {
    return fract(52.9829189 * fract(0.06711056 * pixelCoord.x + 0.00583715 * pixelCoord.y));
}

void main() {
    float rawDepth = texture(uDepthTexture, vTexCoord).r;
    if (rawDepth >= 1.0) {
        FragColor = 1.0;
        return;
    }

    vec3 fragPos = ViewPosFromDepth(vTexCoord);
    vec3 normal = ReconstructNormal(fragPos, vTexCoord);

    // Deterministic random rotation angle from pixel coordinates
    vec2 pixelCoord = vTexCoord * uNoiseScale * 4.0; // uNoiseScale = res/4
    float angle = InterleavedGradientNoise(pixelCoord) * 6.2831853;
    float ca = cos(angle), sa = sin(angle);

    // Build TBN with random rotation in tangent plane
    // Use an arbitrary vector not parallel to normal for tangent basis
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);

    // Apply rotation in tangent plane
    vec3 rotT = tangent * ca + bitangent * sa;
    vec3 rotB = -tangent * sa + bitangent * ca;
    mat3 TBN = mat3(rotT, rotB, normal);

    float occlusion = 0.0;
    int validSamples = 0;

    for (int i = 0; i < uKernelSize; i++) {
        vec3 samplePos = fragPos + TBN * uSamples[i] * uRadius;

        // Project to screen space
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xy = (offset.xy / offset.w) * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        float sampleDepth = ViewPosFromDepth(offset.xy).z;

        // Range check: ignore samples too far from fragment
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
        validSamples++;
    }

    float denom = max(float(validSamples), 1.0);
    FragColor = pow(1.0 - (occlusion / denom), uPower);
}
