#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

uniform mat4 uViewProjection;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
} vs_out;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = uNormalMatrix * aNormal;
    vs_out.TexCoord = aTexCoord;

    // TBN matrix for normal mapping
    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = normalize(vs_out.Normal);
    T = normalize(T - dot(T, N) * N); // re-orthogonalize
    vec3 B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = uViewProjection * worldPos;
}
