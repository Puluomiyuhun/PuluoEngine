#version 450 core

layout(vertices = 4) out;

in vec2 vPosition[];
out vec2 tcPosition[];

uniform vec3  uCamPos;
uniform float uTerrainSize;
uniform vec3  uTerrainOrigin;
uniform float uHeightScale;
uniform sampler2D uHeightmap;

// Must match terrain.tesc exactly to avoid depth mismatch causing self-shadowing
float CalcTessLevel(vec3 worldPos) {
    float dist = distance(worldPos, uCamPos);
    float level = mix(32.0, 2.0, clamp(dist / (uTerrainSize * 0.8), 0.0, 1.0));
    return level;
}

vec3 PatchWorldPos(vec2 normPos) {
    float h = textureLod(uHeightmap, normPos, 0.0).r * uHeightScale;
    return uTerrainOrigin + vec3(normPos.x * uTerrainSize, h, normPos.y * uTerrainSize);
}

void main() {
    tcPosition[gl_InvocationID] = vPosition[gl_InvocationID];

    if (gl_InvocationID == 0) {
        // Patch corners in world space
        vec3 p0 = PatchWorldPos(vPosition[0]);
        vec3 p1 = PatchWorldPos(vPosition[1]);
        vec3 p2 = PatchWorldPos(vPosition[2]);
        vec3 p3 = PatchWorldPos(vPosition[3]);

        // Edge midpoints for tessellation levels
        vec3 e0 = (p0 + p3) * 0.5; // left edge
        vec3 e1 = (p0 + p1) * 0.5; // bottom edge
        vec3 e2 = (p1 + p2) * 0.5; // right edge
        vec3 e3 = (p2 + p3) * 0.5; // top edge
        vec3 center = (p0 + p1 + p2 + p3) * 0.25;

        gl_TessLevelOuter[0] = CalcTessLevel(e0);
        gl_TessLevelOuter[1] = CalcTessLevel(e1);
        gl_TessLevelOuter[2] = CalcTessLevel(e2);
        gl_TessLevelOuter[3] = CalcTessLevel(e3);
        gl_TessLevelInner[0] = CalcTessLevel(center);
        gl_TessLevelInner[1] = CalcTessLevel(center);
    }
}
