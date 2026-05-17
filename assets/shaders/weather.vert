#version 450 core

// Per-vertex (unit quad)
layout(location = 0) in vec2 aQuadPos;   // [-0.5, 0.5]
layout(location = 1) in vec2 aTexCoord;  // [0, 1]

// Per-instance
layout(location = 2) in vec3 aWorldPos;
layout(location = 3) in float aOpacity;

uniform mat4 uViewProjection;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform int  uWeatherType;    // 0=rain, 1=snow
uniform float uSize;
uniform float uStreakLength;
uniform vec3 uFallDirection;  // normalized fall direction (gravity + wind)

out vec2 vTexCoord;
out float vOpacity;

void main() {
    vec3 worldOffset;

    if (uWeatherType == 0) {
        // Rain: camera-facing billboard, stretched vertically along screen
        worldOffset = uCameraRight * aQuadPos.x * uSize
                    + uCameraUp    * aQuadPos.y * uStreakLength;
    } else {
        // Snow: standard camera-aligned billboard
        worldOffset = uCameraRight * aQuadPos.x * uSize
                    + uCameraUp    * aQuadPos.y * uSize;
    }

    vec3 worldPosition = aWorldPos + worldOffset;
    gl_Position = uViewProjection * vec4(worldPosition, 1.0);
    vTexCoord = aTexCoord;
    vOpacity = aOpacity;
}
