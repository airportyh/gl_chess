#version 150

uniform sampler2D tex;
in vec2 fragTexCoord;
out vec4 outputColor;

void main() {
    outputColor = texture(tex, fragTexCoord);
    /*if (fragTexCoord.x > 0.0) {
        outputColor = vec4(0.781, 0.56, 0.277, 1.0);
    } else {
        outputColor = vec4(0.56, 0.28, 0.10, 1.0);
    }*/
    
}