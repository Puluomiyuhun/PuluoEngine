#version 450 core

in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vFragPos;

out vec4 FragColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(vNormal), lightDir), 0.0);
    float ambient = 0.3;
    float lighting = ambient + diff * 0.7;

    vec3 baseColor = vec3(0.6, 0.6, 0.65);
    FragColor = vec4(baseColor * lighting, 1.0);
}
