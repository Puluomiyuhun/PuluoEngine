#version 450 core

// Fullscreen triangle — no VAO needed, use glDrawArrays(GL_TRIANGLES, 0, 3)
// Generates a triangle that covers the entire screen in clip space.

out vec2 vTexCoord;

void main() {
    // Vertex ID 0 → (-1, -1), 1 → (3, -1), 2 → (-1, 3)
    // This forms a single triangle that covers the [-1,1] NDC quad
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vTexCoord = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
