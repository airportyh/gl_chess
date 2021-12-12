#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "errors.h"
#include "read_file.h"

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600
#define SQUARE_LENGTH 0.25

enum PieceType {
    Pawn, Knight, Biship, Rook, King, Queen
};

enum Color {
    Black, White
};

struct Piece {
    enum Color color;
    enum PieceType type;
    int vertexArrayIndex;
};

struct Board {
    struct Piece *squares[64];
};

void displayGLVersions() {
    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
}

int compileShader(char *shaderSource, GLenum shaderType, GLuint *shaderId) {
    (*shaderId) = glCreateShader(shaderType);
    glShaderSource(*shaderId, 1, (const char **)&shaderSource, NULL);
    glCompileShader(*shaderId);
    GLint result;
    glGetShaderiv(*shaderId, GL_COMPILE_STATUS, &result);
    if (!result) {
        char message[1024];
        if (shaderType == GL_VERTEX_SHADER) {
            printf("Compile vertex shader failed.\n");
        } else if (shaderType == GL_FRAGMENT_SHADER) {
            printf("Compile fragment shader failed.\n");
        } else {
            printf("Compile ________ shader failed.\n");
        }
        GLsizei length;
        glGetShaderInfoLog(*shaderId, 1024, &length, message);
        printf("%.*s\n", length, message);
        return 1;
    }
    return 0;
}

void plotSquareInVertices(
    GLfloat x, GLfloat y, 
    GLfloat texTop, GLfloat texLeft, 
    GLfloat texBottom, GLfloat texRight, 
    int idxOffset, GLfloat *vertices
) {
    
    vertices[idxOffset] = x - 1.0;
    vertices[idxOffset + 1] = y - 1.0;
    vertices[idxOffset + 2] = 0.0;
    vertices[idxOffset + 3] = texLeft;
    vertices[idxOffset + 4] = texBottom;
    
    vertices[idxOffset + 5] = x + 1 * SQUARE_LENGTH - 1.0;
    vertices[idxOffset + 6] = y + 1 * SQUARE_LENGTH - 1.0;
    vertices[idxOffset + 7] = 0.0;
    vertices[idxOffset + 8] = texRight;
    vertices[idxOffset + 9] = texTop;
    
    vertices[idxOffset + 10] = x + 1.0 * SQUARE_LENGTH - 1.0;
    vertices[idxOffset + 11] = y + -1.0;
    vertices[idxOffset + 12] = 0.0;
    vertices[idxOffset + 13] = texRight;
    vertices[idxOffset + 14] = texBottom;
    
    vertices[idxOffset + 15] = x - 1.0;
    vertices[idxOffset + 16] = y - 1.0;
    vertices[idxOffset + 17] = 0.0;
    vertices[idxOffset + 18] = texLeft;
    vertices[idxOffset + 19] = texBottom;
    
    vertices[idxOffset + 20] = x - 1.0;
    vertices[idxOffset + 21] = y + 1 * SQUARE_LENGTH - 1.0;
    vertices[idxOffset + 22] = 0.0;
    vertices[idxOffset + 23] = texLeft;
    vertices[idxOffset + 24] = texTop;
    
    vertices[idxOffset + 25] = x + 1 * SQUARE_LENGTH - 1.0;
    vertices[idxOffset + 26] = y + 1 * SQUARE_LENGTH - 1.0;
    vertices[idxOffset + 27] = 0.0;
    vertices[idxOffset + 28] = texRight;
    vertices[idxOffset + 29] = texTop;
}

GLenum textureFormat(int channels) {
    return GL_RGBA;
    GLenum result;
    switch (channels) {
        case 1: result = GL_LUMINANCE;
        case 2: result = GL_LUMINANCE_ALPHA;
        case 3: result = GL_RGB;
        case 4: result = GL_RGBA;
        default: result = -1;
    }
    printf("textureFormat is %d\n", result);
    return result;
}

void setupBoardVertices(GLfloat *boardVertices) {
    GLfloat squareLength = 2.0 / 8.0;
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int offset = (i * 8 + j) * 30;
            GLfloat x = i * squareLength;
            GLfloat y = j * squareLength;
            GLfloat texTop, texLeft, texBottom, texRight;
            if ((i % 2 == 0) ^ (j % 2 == 0)) {
                texTop = 0.0;
                texLeft = 0.0;
                texBottom = 0.5;
                texRight = 1.0 / 7.0;
            } else {
                texTop = 0.5;
                texLeft = 0.0;
                texBottom = 1.0;
                texRight = 1.0 / 7.0;
            }
            plotSquareInVertices(x, y, texTop, texLeft, texBottom, texRight, offset, boardVertices);
        }
    }
}

GLuint setupBoard() {
    GLuint vertexBufferId;
    GLuint vertexArrayId;
    glGenVertexArrays(1, &vertexArrayId);
    glBindVertexArray(vertexArrayId);
    glGenBuffers(1, &vertexBufferId);
    GLfloat boardVertices[64 * 5 * 6];
    
    setupBoardVertices(boardVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 64 * 5 * 6, boardVertices, GL_STATIC_DRAW);

    return vertexArrayId;
}

GLuint setupPieces(GLfloat *piecesVertices) {
    GLuint vertexArrayId;
    glGenVertexArrays(1, &vertexArrayId);
    glBindVertexArray(vertexArrayId);
    GLuint vertexBufferId;
    glGenBuffers(1, &vertexBufferId);
    
    
    plotSquareInVertices(0.0, 0.0, 0.0, 1.0 / 7.0, 0.5, 2.0 / 7.0, 0, piecesVertices);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 32 * 5 * 6, piecesVertices, GL_DYNAMIC_DRAW);

    return vertexArrayId;
}

struct Piece *createPiece(enum Color color, enum PieceType type, int vertexArrayIndex) {
    struct Piece *piece = malloc(sizeof(struct Piece));
    piece->color = color;
    piece->type = type;
    piece->vertexArrayIndex = vertexArrayIndex;
    return piece;
}

/*
The Board
=========

0  1  2  3  4  5  6  7
8  9  10 11 12 13 14 15
16 17 18 19 20 21 22 23
24 25 26 27 28 29 30 31
32 33 34 35 36 37 38 39
40 41 42 43 44 45 46 47
48 49 50 51 52 53 54 55
56 57 58 59 60 61 62 63

*/

void initBoard(struct Board *board) {
    for (int i = 0; i < 64; i++) {
        board->squares[i] = NULL;
    }
    board->squares[0] = createPiece(Black, Rook, 0);
    board->squares[1] = createPiece(Black, Knight, 1);
    board->squares[2] = createPiece(Black, Biship, 2);
}

int appMainLoop() {
    struct Board board;
    
    initBoard(&board);
    
    GLFWwindow* window = NULL;
    
    if (!glfwInit()) {
        return 1;
    }
    
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "MyChess", NULL, NULL);
    
    if (!window) {
        printf("Create window failed.\n");
        return 1;
    } else {
        printf("Create window success.\n");
    }
    
    glfwMakeContextCurrent(window);
    
    if (glewInit() != GLEW_OK) {
        printf("glewInit failed.\n");
        return 1;
    }
    
    displayGLVersions();
    
    char *vertexShaderSource;
    CALL(readFile("vertex_shader.glsl", &vertexShaderSource));
    GLuint vertexShaderId;
    CALL(compileShader(vertexShaderSource, GL_VERTEX_SHADER, &vertexShaderId));
    char *fragmentShaderSource;
    CALL(readFile("fragment_shader.glsl", &fragmentShaderSource));
    GLuint fragmentShaderId;
    CALL(compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER, &fragmentShaderId));
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShaderId);
    glAttachShader(program, fragmentShaderId);
    glLinkProgram(program);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    GLuint textureId;
    GLuint texUniformId;
    int imageWidth, imageHeight, imageChannels;
    unsigned char* imagePixels;
    imagePixels = stbi_load("sprite.png", &imageWidth, &imageHeight, &imageChannels, 0);
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, 
        textureFormat(imageChannels),
        (GLsizei)imageWidth, (GLsizei)imageHeight, 
        0, textureFormat(imageChannels),
        GL_UNSIGNED_BYTE,
        imagePixels);
    stbi_image_free(imagePixels);
    texUniformId = glGetUniformLocation(program, "tex");
    
    GLint vertAttr = glGetAttribLocation(program, "vert");
    GLint vertTexCoordAttr = glGetAttribLocation(program, "vertTexCoord");
    
    GLuint boardVertexArrayId = setupBoard();
    glEnableVertexAttribArray(vertAttr);
    glVertexAttribPointer(vertAttr, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(vertTexCoordAttr);
    glVertexAttribPointer(vertTexCoordAttr, 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));
    
    GLfloat piecesVertices[32 * 5 * 6];
    GLuint numPieces = 1;
        
    GLuint piecesVertexArrayId = setupPieces(piecesVertices);
    
    glEnableVertexAttribArray(vertAttr);
    glVertexAttribPointer(vertAttr, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(vertTexCoordAttr);
    glVertexAttribPointer(vertTexCoordAttr, 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glUseProgram(program);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glUniform1i(texUniformId, 0);
        
        glBindVertexArray(boardVertexArrayId);
        glDrawArrays(GL_TRIANGLES, 0, 64 * 6);
        
        glBindVertexArray(piecesVertexArrayId);
        glDrawArrays(GL_TRIANGLES, 0, 1 * 6);
        
        glfwSwapBuffers(window);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    
    return 0;
}

int main() {
    if (appMainLoop() != 0) {
        printf("initApp failed.\n");
    }
    
    printf("Done.\n");
    return 0;
}