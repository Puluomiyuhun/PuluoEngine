#version 450 core
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aTexCoord;

// Per-instance model matrix (occupies locations 4-7)
layout(location = 4) in mat4 aInstanceModel;

uniform mat4 uViewProjection;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = uViewProjection * aInstanceModel * vec4(aPosition, 1.0);
}
