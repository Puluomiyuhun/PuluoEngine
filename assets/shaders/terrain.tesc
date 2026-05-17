#version 450 core

layout(vertices = 4) out;

in vec2 vPosition[];
out vec2 tcPosition[];

uniform vec3  uCamPos;
uniform float uTerrainSize;
uniform vec3  uTerrainOrigin;
uniform float uHeightScale;
uniform sampler2D uHeightmap;
uniform mat4  uViewProjection;

vec3 PatchWorldPos(vec2 normPos) {
    float h = textureLod(uHeightmap, normPos, 0.0).r * uHeightScale;
    return uTerrainOrigin + vec3(normPos.x * uTerrainSize, h, normPos.y * uTerrainSize);
}

// Frustum culling: check if a sphere is inside the view frustum
bool IsInFrustum(vec3 center, float radius) {
    vec4 c = vec4(center, 1.0);
    // Extract frustum planes from VP matrix (rows of VP^T)
    vec4 r0 = vec4(uViewProjection[0][0], uViewProjection[1][0], uViewProjection[2][0], uViewProjection[3][0]);
    vec4 r1 = vec4(uViewProjection[0][1], uViewProjection[1][1], uViewProjection[2][1], uViewProjection[3][1]);
    vec4 r2 = vec4(uViewProjection[0][2], uViewProjection[1][2], uViewProjection[2][2], uViewProjection[3][2]);
    vec4 r3 = vec4(uViewProjection[0][3], uViewProjection[1][3], uViewProjection[2][3], uViewProjection[3][3]);

    // 6 frustum planes: left, right, bottom, top, near, far
    vec4 planes[6];
    planes[0] = r3 + r0; // left
    planes[1] = r3 - r0; // right
    planes[2] = r3 + r1; // bottom
    planes[3] = r3 - r1; // top
    planes[4] = r3 + r2; // near
    planes[5] = r3 - r2; // far

    for (int i = 0; i < 6; i++) {
        if (dot(planes[i], c) < -radius * length(planes[i].xyz))
            return false;
    }
    return true;
}

// Distance-based tessellation with smooth LOD
float CalcTessLevel(vec3 worldPos) {
    float dist = distance(worldPos, uCamPos);
    float t = clamp(dist / (uTerrainSize * 0.5), 0.0, 1.0);
    // Smooth exponential falloff for better near/far distribution
    float level = mix(32.0, 2.0, t * t);
    return level;
}

void main() {
    tcPosition[gl_InvocationID] = vPosition[gl_InvocationID];

    if (gl_InvocationID == 0) {
        // Patch corners in world space
        vec3 p0 = PatchWorldPos(vPosition[0]);
        vec3 p1 = PatchWorldPos(vPosition[1]);
        vec3 p2 = PatchWorldPos(vPosition[2]);
        vec3 p3 = PatchWorldPos(vPosition[3]);

        vec3 center = (p0 + p1 + p2 + p3) * 0.25;
        // Bounding sphere radius: half-diagonal + height margin
        float patchRadius = max(distance(p0, p2), distance(p1, p3)) * 0.5 + uHeightScale;

        // Frustum cull: skip patches outside view
        if (!IsInFrustum(center, patchRadius)) {
            gl_TessLevelOuter[0] = 0.0;
            gl_TessLevelOuter[1] = 0.0;
            gl_TessLevelOuter[2] = 0.0;
            gl_TessLevelOuter[3] = 0.0;
            gl_TessLevelInner[0] = 0.0;
            gl_TessLevelInner[1] = 0.0;
            return;
        }

        // Edge midpoints for tessellation levels
        vec3 e0 = (p0 + p3) * 0.5; // left edge
        vec3 e1 = (p0 + p1) * 0.5; // bottom edge
        vec3 e2 = (p1 + p2) * 0.5; // right edge
        vec3 e3 = (p2 + p3) * 0.5; // top edge

        gl_TessLevelOuter[0] = CalcTessLevel(e0);
        gl_TessLevelOuter[1] = CalcTessLevel(e1);
        gl_TessLevelOuter[2] = CalcTessLevel(e2);
        gl_TessLevelOuter[3] = CalcTessLevel(e3);
        gl_TessLevelInner[0] = CalcTessLevel(center);
        gl_TessLevelInner[1] = CalcTessLevel(center);
    }
}
