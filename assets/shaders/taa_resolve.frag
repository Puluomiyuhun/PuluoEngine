#version 450 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uCurrentColor;   // Current frame (with jitter applied)
uniform sampler2D uHistoryColor;   // Previous frame TAA output
uniform sampler2D uDepthTexture;   // Current frame depth (for reprojection)

uniform mat4 uCurrentVPInverse;    // (jittered VP)^-1
uniform mat4 uPrevVP;              // Previous frame VP (unjittered)
uniform vec2 uScreenSize;          // Viewport size

void main() {
    vec2 uv = vTexCoord;
    vec3 currentColor = texture(uCurrentColor, uv).rgb;

    // ---- 1. Motion Vector: Reproject current pixel to previous frame ----
    float depth = texture(uDepthTexture, uv).r;

    // Reversed-Z: depth=0 means far plane (sky/infinity).
    // The inverse projection produces w=0 for these pixels, causing NaN.
    // Skip reprojection for sky — use current UV directly.
    vec2 prevUV = uv;
    if (depth > 1e-6) {
        // Reconstruct current frame clip space position
        vec2 ndc = uv * 2.0 - 1.0;
        vec4 clipPos = vec4(ndc, depth, 1.0);

        // Inverse project to world space
        vec4 worldPos = uCurrentVPInverse * clipPos;
        worldPos /= worldPos.w;

        // Project to previous frame screen space
        vec4 prevClip = uPrevVP * worldPos;
        prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;
    }

    // ---- 2. Sample History Color ----
    vec3 historyColor = texture(uHistoryColor, prevUV).rgb;
    // Guard against NaN from corrupted history (can happen after resize/init)
    if (any(isnan(historyColor)) || any(isinf(historyColor))) {
        historyColor = currentColor;
    }

    // ---- 3. Neighborhood Clamp (prevent ghosting) ----
    // Build color bounding box from current frame's 3x3 neighborhood
    vec3 minColor = currentColor;
    vec3 maxColor = currentColor;
    vec2 texelSize = 1.0 / uScreenSize;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0) continue;
            vec3 neighbor = texture(uCurrentColor, uv + vec2(x, y) * texelSize).rgb;
            minColor = min(minColor, neighbor);
            maxColor = max(maxColor, neighbor);
        }
    }

    // Clamp history color to current neighborhood range
    historyColor = clamp(historyColor, minColor, maxColor);

    // ---- 4. Blend ----
    // Check if reprojection is out of screen bounds
    float blend = 0.1;  // Default: 10% current + 90% history
    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        blend = 1.0;  // History invalid, use current frame only
    }

    FragColor = vec4(mix(historyColor, currentColor, blend), 1.0);
}
