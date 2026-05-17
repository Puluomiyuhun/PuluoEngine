#version 450 core

// Per-vertex (unit quad, 6 vertices)
layout(location = 0) in vec2 aQuadPos;   // [-0.5, 0.5]
layout(location = 1) in vec2 aTexCoord;  // [0, 1]

// Per-instance (divisor = 1)
layout(location = 2) in vec3 aWorldPos;
layout(location = 3) in float aSize;
layout(location = 4) in vec4 aColor;

uniform mat4 uViewProjection;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    // Billboard: expand quad in camera-aligned plane
    vec3 worldOffset = uCameraRight * aQuadPos.x * aSize
                     + uCameraUp    * aQuadPos.y * aSize;
    vec3 worldPosition = aWorldPos + worldOffset;

    gl_Position = uViewProjection * vec4(worldPosition, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
