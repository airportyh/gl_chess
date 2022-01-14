#version 330

in vec2 vertex;
in int spriteType;
in float size;
out VS_OUT {
    float size;
    vec2 fragCoordTopLeft;
} vs_out;

void main() {
    gl_Position = vec4(vertex, 0.0, 1.0);
    gl_PointSize = 10.0;
    vs_out.size = size;
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
        // WSquare
        texTop = 0.5;
        texLeft = 0.0;
    } else if (spriteType == 17) {
        // BSquare
        texTop = 0.0;
        texLeft = 0.0;
    } else {
        texTop = 0.5;
        texLeft = 3.0 * pieceWidth;
    }
    
    
    vs_out.fragCoordTopLeft = vec2(texLeft, texTop);
}