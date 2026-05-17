#version 450 core

out float FragColor;

in vec2 vTexCoord;

uniform sampler2D uDepthTexture;
uniform sampler2D uNoiseTexture;

uniform vec3  uSamples[64];
uniform mat4  uProjection;
uniform mat4  uInvProjection;
uniform vec2  uNoiseScale;
uniform int   uKernelSize;
uniform float uRadius;
uniform float uBias;
uniform float uPower;

vec3 ViewPosFromDepth(vec2 uv) {
    float depth = texture(uDepthTexture, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Min-difference normal reconstruction
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

    vec3 n = normalize(cross(dy, dx));
    if (n.z < 0.0) n = -n;
    return n;
}

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
    float linearDepth = -fragPos.z;

    // Fade out at far distances (depth precision too low)
    float distanceFade = smoothstep(200.0, 400.0, linearDepth);

    vec3 normal = ReconstructNormal(fragPos, vTexCoord);

    // At grazing angles (normal nearly perpendicular to view), SSAO is
    // unreliable — the hemisphere flips sideways producing false occlusion.
    // Fade to 1.0 (no AO) when normal.z is small.
    float grazingFade = smoothstep(0.1, 0.4, normal.z);

    // Combined fade
    float fade = grazingFade * (1.0 - distanceFade);
    if (fade <= 0.0) {
        FragColor = 1.0;
        return;
    }

    // Random rotation
    vec2 pixelCoord = vTexCoord * uNoiseScale * 4.0;
    float angle = InterleavedGradientNoise(pixelCoord) * 6.2831853;
    float ca = cos(angle), sa = sin(angle);

    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    vec3 rotT = tangent * ca + bitangent * sa;
    vec3 rotB = -tangent * sa + bitangent * ca;
    mat3 TBN = mat3(rotT, rotB, normal);

    float occlusion = 0.0;

    for (int i = 0; i < uKernelSize; i++) {
        vec3 samplePos = fragPos + TBN * uSamples[i] * uRadius;

        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xy = (offset.xy / offset.w) * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        float sampleDepth = ViewPosFromDepth(offset.xy).z;

        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    float ao = pow(1.0 - (occlusion / float(uKernelSize)), uPower);

    // Apply fade: grazing angles and far distance → no AO
    FragColor = mix(1.0, ao, fade);
}
