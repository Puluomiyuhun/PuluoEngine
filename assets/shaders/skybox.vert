#version 450 core
layout(location = 0) in vec3 aPos;

out vec3 vLocalPos;

uniform mat4 uProjection;
uniform mat4 uView;

void main() {
    vLocalPos = aPos;
    // Remove translation from view matrix
    mat4 rotView = mat4(mat3(uView));
    vec4 clipPos = uProjection * rotView * vec4(aPos, 1.0);
    // Set z = w so depth = 1.0 (rendered behind everything)
    gl_Position = clipPos.xyww;
}
