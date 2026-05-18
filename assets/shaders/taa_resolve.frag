#version 450 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uCurrentColor;   // Current frame (with jitter applied)
uniform sampler2D uHistoryColor;   // Previous frame TAA output
uniform sampler2D uDepthTexture;   // Current frame depth (for reprojection)

uniform mat4 uCurrentVPInverse;    // (jittered VP)^-1
uniform mat4 uPrevVP;              // Previous frame VP (unjittered)
uniform vec2 uScreenSize;          // Viewport size
uniform vec2 uJitter;              // Current frame jitter in pixel units

// ---- YCoCg color space for stable variance clipping ----
vec3 RGBToYCoCg(vec3 rgb) {
    return vec3(
         0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
         0.5  * rgb.r                - 0.5  * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

vec3 YCoCgToRGB(vec3 ycocg) {
    float y  = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;
    return vec3(
        y + co - cg,
        y      + cg,
        y - co - cg
    );
}

// Clip point towards AABB center (Salvi 2016 variance clipping)
vec3 ClipAABB(vec3 aabbMin, vec3 aabbMax, vec3 point) {
    vec3 center = 0.5 * (aabbMin + aabbMax);
    vec3 extents = 0.5 * (aabbMax - aabbMin) + 1e-7;
    vec3 offset = point - center;
    vec3 ts = abs(offset / extents);
    float t = max(ts.x, max(ts.y, ts.z));
    if (t > 1.0)
        return center + offset / t;
    return point;
}

void main() {
    vec2 uv = vTexCoord;
    vec2 texelSize = 1.0 / uScreenSize;

    // ---- 1. Unjittered current color (sample at pixel center minus jitter) ----
    // The scene was rendered with jitter, so the "true" color for this pixel
    // center is at uv - jitter. This removes per-frame spatial shift.
    vec3 currentColor = texture(uCurrentColor, uv).rgb;

    // ---- 2. Motion Vector: Reproject current pixel to previous frame ----
    float depth = texture(uDepthTexture, uv).r;

    // Reversed-Z: depth=0 means far plane (sky/infinity).
    // Inverse projection produces w=0 → NaN. Use current UV directly.
    vec2 prevUV = uv;
    if (depth > 1e-6) {
        vec2 ndc = uv * 2.0 - 1.0;
        vec4 clipPos = vec4(ndc, depth, 1.0);
        vec4 worldPos = uCurrentVPInverse * clipPos;
        worldPos /= worldPos.w;
        vec4 prevClip = uPrevVP * worldPos;
        prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;
    }

    // ---- 3. Sample History ----
    vec3 historyColor = texture(uHistoryColor, prevUV).rgb;
    if (any(isnan(historyColor)) || any(isinf(historyColor))) {
        historyColor = currentColor;
    }

    // ---- 4. Variance Clipping in YCoCg space (Salvi 2016) ----
    // Much more stable than RGB min/max: uses mean ± gamma*stddev
    vec3 m1 = vec3(0.0);  // first moment (sum)
    vec3 m2 = vec3(0.0);  // second moment (sum of squares)

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 s = RGBToYCoCg(texture(uCurrentColor, uv + vec2(x, y) * texelSize).rgb);
            m1 += s;
            m2 += s * s;
        }
    }

    vec3 mean = m1 / 9.0;
    vec3 variance = abs(m2 / 9.0 - mean * mean);
    vec3 stddev = sqrt(variance);

    float gamma = 1.25;  // Clipping aggressiveness (1.0=tight, 2.0=loose)
    vec3 aabbMin = mean - gamma * stddev;
    vec3 aabbMax = mean + gamma * stddev;

    vec3 historyYCoCg = RGBToYCoCg(historyColor);
    historyYCoCg = ClipAABB(aabbMin, aabbMax, historyYCoCg);
    historyColor = YCoCgToRGB(historyYCoCg);

    // ---- 5. Adaptive Blend ----
    float blend = 0.05;  // Default: 5% current + 95% history (more stable)

    // Out-of-bounds reprojection: use current frame only
    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        blend = 1.0;
    }

    // Increase blend for high-variance areas (thin geometry, alpha-tested edges)
    float luminanceVariance = variance.x;  // Y channel variance
    blend = mix(blend, 0.2, smoothstep(0.001, 0.05, luminanceVariance));

    FragColor = vec4(mix(historyColor, currentColor, blend), 1.0);
}
