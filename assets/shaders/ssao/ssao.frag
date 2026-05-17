#version 450 core

out float FragColor;

in vec2 vTexCoord;

uniform sampler2D uDepthTexture;
uniform sampler2D uNoiseTexture;   // kept for compatibility, not sampled

uniform vec3  uSamples[64];
uniform mat4  uProjection;
uniform mat4  uInvProjection;
uniform vec2  uNoiseScale;         // (width/4, height/4)
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

// Stable normal reconstruction: min-difference neighbor pairs
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

    vec3 dx = (abs(dxLeft.z) < abs(dxRight.z)) ? dxLeft : dxRight;
    vec3 dy = (abs(dyDown.z) < abs(dyUp.z))    ? dyDown : dyUp;

    vec3 normal = normalize(cross(dy, dx));
    if (normal.z < 0.0) normal = -normal;
    return normal;
}

// Interleaved gradient noise — deterministic per-pixel, no tiling
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
    float linearDepth = -fragPos.z; // positive distance from camera

    // Fade out SSAO at far distances (depth precision degrades)
    float distanceFade = smoothstep(200.0, 400.0, linearDepth);
    if (distanceFade >= 1.0) {
        FragColor = 1.0;
        return;
    }

    vec3 normal = ReconstructNormal(fragPos, vTexCoord);

    // At grazing angles, normal.z is near zero → hemisphere flips sideways
    // causing false occlusion. Bias normal toward camera (view dir = 0,0,1)
    // to keep hemisphere stable.
    const float MIN_NZ = 0.3;
    if (normal.z < MIN_NZ) {
        normal.z = MIN_NZ;
        normal = normalize(normal);
    }

    // Deterministic random rotation angle from pixel coordinates
    vec2 pixelCoord = vTexCoord * uNoiseScale * 4.0;
    float angle = InterleavedGradientNoise(pixelCoord) * 6.2831853;
    float ca = cos(angle), sa = sin(angle);

    // Build TBN with random rotation in tangent plane
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    vec3 rotT = tangent * ca + bitangent * sa;
    vec3 rotB = -tangent * sa + bitangent * ca;
    mat3 TBN = mat3(rotT, rotB, normal);

    // Scale radius by depth so AO detail is consistent across distances
    float scaledRadius = uRadius * (1.0 + linearDepth * 0.02);
    // But cap it so it doesn't get absurdly large
    scaledRadius = min(scaledRadius, uRadius * 5.0);

    float occlusion = 0.0;

    for (int i = 0; i < uKernelSize; i++) {
        vec3 samplePos = fragPos + TBN * uSamples[i] * scaledRadius;

        // Project to screen space
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xy = (offset.xy / offset.w) * 0.5 + 0.5;

        // Out-of-bounds → assume no occlusion (counts as 0 in numerator, 1 in denominator)
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        float sampleDepth = ViewPosFromDepth(offset.xy).z;

        float rangeCheck = smoothstep(0.0, 1.0, scaledRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    // Divide by total kernel size — off-screen samples = no occlusion
    float ao = pow(1.0 - (occlusion / float(uKernelSize)), uPower);

    // Blend toward 1.0 (no occlusion) at far distances
    FragColor = mix(ao, 1.0, distanceFade);
}
