#version 330

layout (points) in;
layout (triangle_strip, max_vertices=4) out;
in VS_OUT {
    float size;
} gs_in[];
uniform mat4 perspective;

out vec2 fragTexCoord;

void main() {
    vec4 position = gl_in[0].gl_Position;
    float size = gs_in[0].size;
    gl_Position = perspective * (position + vec4(0, size, 0, 0));
    fragTexCoord = vec2(0, 1);
    EmitVertex();
    
    gl_Position = perspective * (position + vec4(size, size, 0, 0));
    fragTexCoord = vec2(1, 1);
    EmitVertex();
    
    gl_Position = perspective * position;
    fragTexCoord = vec2(0, 0);
    EmitVertex();
    
    gl_Position = perspective * (position + vec4(size, 0, 0, 0));
    fragTexCoord = vec2(1, 0);
    EmitVertex();
    
    
    EndPrimitive();
}