#version 450 core

layout(quads, fractional_odd_spacing, ccw) in;

in vec2 tcPosition[];

uniform sampler2D uHeightmap;
uniform float     uHeightScale;
uniform float     uTerrainSize;
uniform vec3      uTerrainOrigin;
uniform mat4      uLightSpaceMatrix;

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Bilinear interpolation of patch corners
    vec2 bot = mix(tcPosition[0], tcPosition[1], u);
    vec2 top = mix(tcPosition[3], tcPosition[2], u);
    vec2 pos = mix(bot, top, v);

    // Sample height
    float height = textureLod(uHeightmap, pos, 0.0).r * uHeightScale;

    // World position
    vec3 worldPos = uTerrainOrigin + vec3(pos.x * uTerrainSize, height, pos.y * uTerrainSize);

    gl_Position = uLightSpaceMatrix * vec4(worldPos, 1.0);
}
