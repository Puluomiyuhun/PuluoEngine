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

// Reconstruct view-space position from depth
vec3 ViewPosFromDepth(vec2 uv) {
    float depth = texture(uDepthTexture, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    vec3 fragPos = ViewPosFromDepth(vTexCoord);

    // Skip far plane fragments (no geometry)
    float rawDepth = texture(uDepthTexture, vTexCoord).r;
    if (rawDepth >= 1.0) {
        FragColor = 1.0;
        return;
    }

    // Reconstruct normal from depth cross-derivatives
    vec3 dPdx = dFdx(fragPos);
    vec3 dPdy = dFdy(fragPos);
    vec3 normal = normalize(cross(dPdy, dPdx));
    // Ensure normal points toward camera (view-space Z is negative)
    if (normal.z < 0.0) normal = -normal;

    // Random rotation vector from tiled noise texture
    vec3 randomVec = normalize(texture(uNoiseTexture, vTexCoord * uNoiseScale).xyz);

    // Gram-Schmidt to build TBN oriented to surface normal
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < uKernelSize; i++) {
        // Offset sample position in view space
        vec3 samplePos = fragPos + TBN * uSamples[i] * uRadius;

        // Project sample to screen space
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xy = (offset.xy / offset.w) * 0.5 + 0.5;

        // Clamp to valid UV range
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        // Sample depth at projected position
        float sampleDepth = ViewPosFromDepth(offset.xy).z;

        // Range check: ignore samples too far from fragment
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    FragColor = pow(1.0 - (occlusion / float(uKernelSize)), uPower);
}
