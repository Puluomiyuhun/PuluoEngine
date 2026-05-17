#version 450 core

layout(quads, fractional_odd_spacing, cw) in;

in vec2 tcPosition[];

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec2 vTexCoord;

uniform sampler2D uHeightmap;
uniform float     uHeightScale;
uniform float     uTerrainSize;
uniform vec3      uTerrainOrigin;
uniform float     uUVScale;
uniform mat4      uViewProjection;

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Bilinear interpolation of patch corners
    // tcPosition layout: [0]=BL, [1]=BR, [2]=TR, [3]=TL
    vec2 bot = mix(tcPosition[0], tcPosition[1], u);
    vec2 top = mix(tcPosition[3], tcPosition[2], u);
    vec2 pos = mix(bot, top, v);

    // Sample height
    float height = textureLod(uHeightmap, pos, 0.0).r * uHeightScale;

    // World position
    vec3 worldPos = uTerrainOrigin + vec3(pos.x * uTerrainSize, height, pos.y * uTerrainSize);

    // Compute normal from heightmap gradients (central difference)
    float texelSize = 1.0 / float(textureSize(uHeightmap, 0).x);
    float hL = textureLod(uHeightmap, pos + vec2(-texelSize, 0.0), 0.0).r * uHeightScale;
    float hR = textureLod(uHeightmap, pos + vec2( texelSize, 0.0), 0.0).r * uHeightScale;
    float hD = textureLod(uHeightmap, pos + vec2(0.0, -texelSize), 0.0).r * uHeightScale;
    float hU = textureLod(uHeightmap, pos + vec2(0.0,  texelSize), 0.0).r * uHeightScale;

    float worldTexelSize = uTerrainSize * texelSize;
    vec3 normal = normalize(vec3(hL - hR, 2.0 * worldTexelSize, hD - hU));

    // Tangent and bitangent from heightmap (for TBN normal mapping)
    vec3 tangent = normalize(vec3(2.0 * worldTexelSize, hR - hL, 0.0));
    vec3 bitangent = normalize(vec3(0.0, hU - hD, 2.0 * worldTexelSize));

    vWorldPos = worldPos;
    vNormal = normal;
    vTangent = tangent;
    vBitangent = bitangent;
    vTexCoord = pos * uUVScale;

    gl_Position = uViewProjection * vec4(worldPos, 1.0);
}
