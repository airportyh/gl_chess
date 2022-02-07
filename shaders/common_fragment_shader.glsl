#version 330

uniform sampler2D tex;
in vec2 fragTexCoord;
out vec4 outputColor;

void main() {
    outputColor = texture(tex, fragTexCoord);
    //outputColor = vec4(1.0, 0.0, 0.0, 1.0);
}