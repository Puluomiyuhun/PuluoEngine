#version 450 core
layout(location = 0) in vec3 aPos;

out vec3 vLocalPos;

uniform mat4 uProjection;
uniform mat4 uView;

void main() {
    vLocalPos = aPos;
    mat4 rotView = mat4(mat3(uView));
    vec4 clipPos = uProjection * rotView * vec4(aPos, 1.0);
    // Reversed-Z: z = 0 → depth 0.0 (far plane)
    gl_Position = vec4(clipPos.xy, 0.0, clipPos.w);
}
