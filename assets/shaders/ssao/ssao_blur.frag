#version 450 core

out float FragColor;

in vec2 vTexCoord;

uniform sampler2D uSSAOInput;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uSSAOInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            result += texture(uSSAOInput, vTexCoord + vec2(float(x), float(y)) * texelSize).r;
        }
    }
    FragColor = result / 16.0;
}
