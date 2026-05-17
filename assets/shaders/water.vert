#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

uniform mat4 uViewProjection;
uniform mat4 uModel;

out vec3 vWorldPos;
out vec2 vTexCoord;
out vec4 vClipPos;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vTexCoord = aTexCoord;
    vClipPos = uViewProjection * worldPos;
    gl_Position = vClipPos;
}
