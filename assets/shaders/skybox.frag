#version 450 core

in vec3 vLocalPos;
out vec4 FragColor;

uniform samplerCube uEnvironmentMap;

void main() {
    vec3 envColor = texture(uEnvironmentMap, vLocalPos).rgb;

    // HDR tonemap + gamma
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0 / 2.2));

    FragColor = vec4(envColor, 1.0);
}
