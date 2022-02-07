#version 330

in vec2 vertex;
in float size;

out VS_OUT {
    float size;
} vs_out;

void main() {
    gl_PointSize = 20;
    gl_Position = vec4(vertex, 0, 1.0);
    vs_out.size = size;
}