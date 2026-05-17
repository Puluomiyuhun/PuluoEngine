#version 450 core

layout(location = 0) in vec2 aPosition; // normalized [0,1] XZ

out vec2 vPosition;

void main() {
    vPosition = aPosition;
}
