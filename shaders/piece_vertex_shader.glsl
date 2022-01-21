#version 330

uniform float boardSize;
uniform vec2 boardTopLeft;
uniform int overrideID;
uniform vec2 overridePosition;
in int spriteType;

out VS_OUT {
    float size;
    vec2 fragCoordTopLeft;
} vs_out;

void main() {
    int boardPos = gl_VertexID;
    vec2 vertex;
    if (boardPos == overrideID) {
        vertex = boardTopLeft + overridePosition;        
    }  else {
        vertex = boardTopLeft + (vec2(mod(boardPos, 8), 7 - boardPos / 8) * boardSize / 8);
    }
    gl_Position = vec4(vertex, 0.0, 1.0);
    vs_out.size = boardSize / 8;
    float texTop;
    float texLeft;
    float pieceWidth = 1.0 / 7.0;
    
    if (spriteType == 0) {
        // WPawn
        texTop = 0.5;
        texLeft = 1.0 * pieceWidth;
    } else if (spriteType == 1) {
        // WKnight
        texTop = 0.5;
        texLeft = 2.0 * pieceWidth;
    } else if (spriteType == 2) {
        // WBiship
        texTop = 0.5;
        texLeft = 3.0 * pieceWidth;
    } else if (spriteType == 3) {
        // WRook
        texTop = 0.5;
        texLeft = 4.0 * pieceWidth;
    } else if (spriteType == 4) {
        // WKing
        texTop = 0.5;
        texLeft = 5.0 * pieceWidth;
    } else if (spriteType == 5) {
        // WQueen
        texTop = 0.5;
        texLeft = 6.0 * pieceWidth;
    } else if (spriteType == 8) {
        // BPawn
        texTop = 0.0;
        texLeft = 1.0 * pieceWidth;
    } else if (spriteType == 9) {
        // BKnight
        texTop = 0.0;
        texLeft = 2.0 * pieceWidth;
    } else if (spriteType == 10) {
        // BBiship
        texTop = 0.0;
        texLeft = 3.0 * pieceWidth;
    } else if (spriteType == 11) {
        // BRook
        texTop = 0.0;
        texLeft = 4.0 * pieceWidth;
    } else if (spriteType == 12) {
        // BKing
        texTop = 0.0;
        texLeft = 5.0 * pieceWidth;
    } else if (spriteType == 13) {
        // BQueen
        texTop = 0.0;
        texLeft = 6.0 * pieceWidth;
    } else if (spriteType == 16) {
        // Blank
        vs_out.size = 0;
    } else {
        vs_out.size = 0;
    }
    
    vs_out.fragCoordTopLeft = vec2(texLeft, texTop);
}