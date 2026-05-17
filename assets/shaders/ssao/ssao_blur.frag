#version 450 core

out float FragColor;

in vec2 vTexCoord;

uniform sampler2D uSSAOInput;
uniform sampler2D uDepthTexture;
uniform vec2  uDirection;  // (1,0) for horizontal, (0,1) for vertical

const int   BLUR_RADIUS = 4;
const float DEPTH_THRESHOLD = 0.001; // relative depth difference threshold

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uSSAOInput, 0));
    float centerVal  = texture(uSSAOInput, vTexCoord).r;
    float centerDepth = texture(uDepthTexture, vTexCoord).r;

    // Skip far plane
    if (centerDepth >= 1.0) {
        FragColor = 1.0;
        return;
    }

    float totalWeight = 1.0;
    float totalValue  = centerVal;

    for (int i = 1; i <= BLUR_RADIUS; ++i) {
        float fi = float(i);
        // Gaussian-like weight: sigma ≈ BLUR_RADIUS/2
        float spatialW = exp(-0.5 * (fi * fi) / (float(BLUR_RADIUS) * 0.5));

        for (int sign = -1; sign <= 1; sign += 2) {
            vec2 offset = uDirection * texelSize * fi * float(sign);
            vec2 uv = vTexCoord + offset;

            float sampleVal   = texture(uSSAOInput, uv).r;
            float sampleDepth = texture(uDepthTexture, uv).r;

            // Bilateral weight: reject samples across depth discontinuities
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthW = step(depthDiff, DEPTH_THRESHOLD);

            float w = spatialW * depthW;
            totalValue  += sampleVal * w;
            totalWeight += w;
        }
    }

    FragColor = totalValue / totalWeight;
}
