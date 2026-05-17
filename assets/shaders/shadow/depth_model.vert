#version 450 core
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPosition, 1.0);
}
