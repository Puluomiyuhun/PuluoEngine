#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

// Per-instance model matrix (occupies locations 4-7)
layout(location = 4) in mat4 aInstanceModel;

uniform mat4 uViewProjection;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
} vs_out;

void main() {
    mat3 normalMatrix = transpose(inverse(mat3(aInstanceModel)));

    vec4 worldPos = aInstanceModel * vec4(aPosition, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = normalMatrix * aNormal;
    vs_out.TexCoord = aTexCoord;

    // TBN matrix for normal mapping
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(vs_out.Normal);
    T = normalize(T - dot(T, N) * N); // re-orthogonalize
    vec3 B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = uViewProjection * worldPos;
}
