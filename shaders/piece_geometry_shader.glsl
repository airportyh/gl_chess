#version 330

layout (points) in;
layout (triangle_strip, max_vertices=4) out;
in VS_OUT {
    float size;
    vec2 fragCoordTopLeft;
} gs_in[];
uniform mat4 perspective;

out vec2 fragTexCoord;

void main() {
    float pieceLength = 1.0 / 7.0;
    vec4 position = gl_in[0].gl_Position;
    float size = gs_in[0].size;
    
    if (size == 0) {
        return;
    }
    
    vec2 fragCoordTopLeft = gs_in[0].fragCoordTopLeft;
    //fragCoordTopLeft = vec2(2.0 / 7.0, 0.5);
    gl_Position = perspective * (position + vec4(0, size, 0, 0));
    fragTexCoord = fragCoordTopLeft + vec2(0, 0.5);
    EmitVertex();
    
    gl_Position = perspective * (position + vec4(size, size, 0, 0));
    fragTexCoord = fragCoordTopLeft + vec2(pieceLength, 0.5);
    EmitVertex();
    
    gl_Position = perspective * position;
    fragTexCoord = fragCoordTopLeft;
    EmitVertex();
    
    gl_Position = perspective * (position + vec4(size, 0, 0, 0));
    fragTexCoord = fragCoordTopLeft + vec2(pieceLength, 0);
    EmitVertex();
    
    EndPrimitive();
}

