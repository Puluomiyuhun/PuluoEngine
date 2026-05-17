#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

uniform mat4 uViewProjection;

out vec3 vColor;
out vec3 vWorldPos;

void main() {
    vColor = aColor;
    vWorldPos = aPosition;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
