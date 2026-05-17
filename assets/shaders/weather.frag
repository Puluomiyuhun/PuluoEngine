#version 450 core

in vec2 vTexCoord;
in float vOpacity;

uniform int  uWeatherType;  // 0=rain, 1=snow
uniform vec4 uColor;

out vec4 FragColor;

void main() {
    float alpha;

    if (uWeatherType == 0) {
        // Rain: vertical streak — bright center, fade at top/bottom
        float centerDist = abs(vTexCoord.y - 0.5) * 2.0; // 0 at center, 1 at edge
        float xDist = abs(vTexCoord.x - 0.5) * 2.0;
        alpha = (1.0 - smoothstep(0.0, 1.0, centerDist))
              * (1.0 - smoothstep(0.3, 1.0, xDist));
    } else {
        // Snow: soft circle
        float dist = length(vTexCoord - vec2(0.5));
        alpha = 1.0 - smoothstep(0.2, 0.5, dist);
    }

    alpha *= vOpacity * uColor.a;
    FragColor = vec4(uColor.rgb, alpha);
}
