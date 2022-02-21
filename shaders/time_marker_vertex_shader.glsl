#version 330

in vec2 pos;
uniform mat4 perspective;

void main() {
    gl_Position = perspective * vec4(pos.xy, 0.0, 1.0);
}