#version 450 core

in vec2 vTexCoord;

uniform sampler2D uMaskMap;
uniform bool  uUseAlphaMask;
uniform float uAlphaCutoff;

void main() {
    if (uUseAlphaMask) {
        float mask = texture(uMaskMap, vTexCoord).r;
        if (mask < uAlphaCutoff) discard;
    }
    // Depth written automatically
}
