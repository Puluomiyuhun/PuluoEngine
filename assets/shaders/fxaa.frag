#version 450 core

// FXAA 3.11 — Quality preset (based on Timothy Lottes / NVIDIA FXAA white paper)
// Adapted for single-pass post-process usage.

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uScreenTexture;
uniform vec2 uInverseScreenSize; // 1.0 / vec2(width, height)

// SSR composite (optional)
uniform int uSSREnabled;
uniform sampler2D uSSRTexture;

// Color grading
uniform float uSaturation; // 0=grayscale, 1=normal, >1=oversaturated
uniform float uContrast;   // 0.5=low, 1=normal, 2=high

// FXAA quality parameters
#define EDGE_THRESHOLD_MIN 0.0312  // Skip very dark edges (invisible aliasing)
#define EDGE_THRESHOLD_MAX 0.125   // Skip low-contrast edges
#define SUBPIXEL_QUALITY   0.75    // Subpixel anti-aliasing strength (0=off, 1=full)
#define SEARCH_STEPS       12      // Max steps along edge in each direction

// Quality offsets for each search step (how far to jump)
const float QUALITY[12] = float[12](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

// Convert RGB to perceptual luma (fast approximation)
float Luma(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = vTexCoord;
    vec2 texel = uInverseScreenSize;

    // ---- 1. Sample center and 4 neighbors, compute luma ----
    vec3 colorCenter = texture(uScreenTexture, uv).rgb;

    // Blend SSR reflections into scene color before FXAA processing
    if (uSSREnabled == 1) {
        vec4 ssr = texture(uSSRTexture, uv);
        colorCenter = mix(colorCenter, ssr.rgb, ssr.a);
    }

    float lumaCenter = Luma(colorCenter);

    float lumaDown  = Luma(texture(uScreenTexture, uv + vec2( 0.0, -1.0) * texel).rgb);
    float lumaUp    = Luma(texture(uScreenTexture, uv + vec2( 0.0,  1.0) * texel).rgb);
    float lumaLeft  = Luma(texture(uScreenTexture, uv + vec2(-1.0,  0.0) * texel).rgb);
    float lumaRight = Luma(texture(uScreenTexture, uv + vec2( 1.0,  0.0) * texel).rgb);

    // ---- 2. Compute local contrast (edge detection) ----
    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    // Skip if contrast is too low (no visible aliasing) or too dark
    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX)) {
        vec3 c = colorCenter;
        float g = Luma(c);
        c = mix(vec3(g), c, uSaturation);
        c = (c - 0.5) * uContrast + 0.5;
        FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
        return;
    }

    // ---- 3. Sample 4 diagonal neighbors for subpixel aliasing ----
    float lumaDL = Luma(texture(uScreenTexture, uv + vec2(-1.0, -1.0) * texel).rgb);
    float lumaDR = Luma(texture(uScreenTexture, uv + vec2( 1.0, -1.0) * texel).rgb);
    float lumaUL = Luma(texture(uScreenTexture, uv + vec2(-1.0,  1.0) * texel).rgb);
    float lumaUR = Luma(texture(uScreenTexture, uv + vec2( 1.0,  1.0) * texel).rgb);

    float lumaDownUp   = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;

    // ---- 4. Determine edge direction (horizontal vs vertical) ----
    float lumaLeftCorners  = lumaDL + lumaUL;
    float lumaDownCorners  = lumaDL + lumaDR;
    float lumaRightCorners = lumaDR + lumaUR;
    float lumaUpCorners    = lumaUL + lumaUR;

    // Horizontal edge: large vertical gradient
    float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) +
                           abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 +
                           abs(-2.0 * lumaRight + lumaRightCorners);
    // Vertical edge: large horizontal gradient
    float edgeVertical   = abs(-2.0 * lumaUp + lumaUpCorners) +
                           abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 +
                           abs(-2.0 * lumaDown + lumaDownCorners);

    bool isHorizontal = (edgeHorizontal >= edgeVertical);

    // ---- 5. Select edge direction and compute gradient ----
    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp   : lumaRight;

    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;

    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    // Step perpendicular to the edge (in the direction of steepest gradient)
    float stepLength = isHorizontal ? texel.y : texel.x;
    float lumaLocalAvg;

    if (is1Steepest) {
        stepLength = -stepLength;
        lumaLocalAvg = 0.5 * (luma1 + lumaCenter);
    } else {
        lumaLocalAvg = 0.5 * (luma2 + lumaCenter);
    }

    // Move UV half a texel in the perpendicular direction (onto the edge)
    vec2 currentUV = uv;
    if (isHorizontal) {
        currentUV.y += stepLength * 0.5;
    } else {
        currentUV.x += stepLength * 0.5;
    }

    // ---- 6. Search along the edge in both directions ----
    // Offset direction: along the edge
    vec2 offset = isHorizontal ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);

    vec2 uv1 = currentUV - offset;
    vec2 uv2 = currentUV + offset;

    float lumaEnd1 = Luma(texture(uScreenTexture, uv1).rgb) - lumaLocalAvg;
    float lumaEnd2 = Luma(texture(uScreenTexture, uv2).rgb) - lumaLocalAvg;

    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;

    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;

    // Continue searching if we haven't found both endpoints
    if (!reachedBoth) {
        for (int i = 2; i < SEARCH_STEPS; i++) {
            if (!reached1) {
                lumaEnd1 = Luma(texture(uScreenTexture, uv1).rgb) - lumaLocalAvg;
                reached1 = abs(lumaEnd1) >= gradientScaled;
            }
            if (!reached2) {
                lumaEnd2 = Luma(texture(uScreenTexture, uv2).rgb) - lumaLocalAvg;
                reached2 = abs(lumaEnd2) >= gradientScaled;
            }
            reachedBoth = reached1 && reached2;
            if (!reached1) uv1 -= offset * QUALITY[i];
            if (!reached2) uv2 += offset * QUALITY[i];
            if (reachedBoth) break;
        }
    }

    // ---- 7. Compute final UV offset ----
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeLength = distance1 + distance2;

    // Pixel offset along the perpendicular direction
    float pixelOffset = -distanceFinal / edgeLength + 0.5;

    // Check if the endpoints have the right sign
    bool isLumaCenterSmaller = lumaCenter < lumaLocalAvg;
    bool correctVariation1 = (lumaEnd1 < 0.0) != isLumaCenterSmaller;
    bool correctVariation2 = (lumaEnd2 < 0.0) != isLumaCenterSmaller;
    bool correctVariation = isDirection1 ? correctVariation1 : correctVariation2;

    float finalOffset = correctVariation ? pixelOffset : 0.0;

    // ---- 8. Subpixel anti-aliasing (smooths single-pixel features) ----
    float lumaAvg = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subpixelOffset1 = clamp(abs(lumaAvg - lumaCenter) / lumaRange, 0.0, 1.0);
    float subpixelOffset2 = (-2.0 * subpixelOffset1 + 3.0) * subpixelOffset1 * subpixelOffset1;
    float subpixelOffset = subpixelOffset2 * subpixelOffset2 * SUBPIXEL_QUALITY;

    // Use the larger of the two offsets
    finalOffset = max(finalOffset, subpixelOffset);

    // ---- 9. Final sample with offset ----
    vec2 finalUV = uv;
    if (isHorizontal) {
        finalUV.y += finalOffset * stepLength;
    } else {
        finalUV.x += finalOffset * stepLength;
    }

    FragColor = vec4(texture(uScreenTexture, finalUV).rgb, 1.0);

    // Apply SSR to final output as well
    if (uSSREnabled == 1) {
        vec4 ssr = texture(uSSRTexture, finalUV);
        FragColor.rgb = mix(FragColor.rgb, ssr.rgb, ssr.a);
    }

    // Color grading: saturation then contrast
    float gray = Luma(FragColor.rgb);
    FragColor.rgb = mix(vec3(gray), FragColor.rgb, uSaturation);
    FragColor.rgb = (FragColor.rgb - 0.5) * uContrast + 0.5;
    FragColor.rgb = clamp(FragColor.rgb, 0.0, 1.0);
}
