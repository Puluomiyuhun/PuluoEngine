#version 450 core

in vec2 vTexCoord;
in vec4 vColor;

out vec4 FragColor;

void main() {
    // Soft circle falloff (procedural, no texture needed)
    float dist = length(vTexCoord - vec2(0.5));
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);

    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
