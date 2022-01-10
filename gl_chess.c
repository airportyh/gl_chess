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

struct BoardRender {
    GLfloat top;
    GLfloat left;
    GLfloat width;
    GLfloat height;
    GLfloat boardVertices[64 * 5 * 6];
    GLuint boardVertexArrayId;
    GLuint boardVertexBufferId;
    GLfloat piecesVertices[32 * 5 * 6];
    GLuint piecesVertexArrayId;
    GLuint piecesVertexBufferId;
};

struct Board board;
GLfloat piecesVertices[32 * 5 * 6];
static int draggingSquare = -1;
GLuint piecesVertexArrayId;
GLuint piecesVertexBufferId;
GLuint boardVertexArrayId;

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

void clearPieceVertex(int idxOffset, GLfloat *vertices) {
    for (int i = 0; i < 30; i++) {
        vertices[idxOffset + i] = 0.0;
    }
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

void plotPieceInVertices(int position, struct Piece *piece, GLfloat *vertices) {
    GLfloat pieceWidth = 1.0 / 7.0;
    GLfloat texTop, texLeft, texBottom, texRight;
    if (piece->color == Black) {
        texTop = 0.0;
        texBottom = 0.5;
    } else { // White
        texTop = 0.5;
        texBottom = 1.0;
    }
    
    switch (piece->type) {
        case Pawn:
            texLeft = 1.0 * pieceWidth;
            break;
        case Knight:
            texLeft = 2.0 * pieceWidth;
            break;
        case Biship:
            texLeft = 3.0 * pieceWidth;
            break;
        case Rook:
            texLeft = 4.0 * pieceWidth;
            break;
        case King:
            texLeft = 5.0 * pieceWidth;
            break;
        case Queen:
            texLeft = 6.0 * pieceWidth;
            break;
    }
    
    texRight = texLeft + pieceWidth;
    
    GLfloat squareLength = 2.0 / 8.0;
    GLfloat x = squareLength * (position % 8);
    GLfloat y = squareLength * (7 - floor(position / 8));
    
    printf("Plotting type %d at x = %f, y = %f\n", piece->type, x, y);
    // printf("texTop = %f, texLeft = %f, texBottom = %f, texRight = %f\n", texTop, texLeft, texBottom, texRight);
    
    plotSquareInVertices(x, y, texTop, texLeft, texBottom, texRight, piece->vertexArrayIndex * 30, vertices);
}

void plotPieceXYInVertices(double x, double y, struct Piece *piece, GLfloat *vertices) {
    GLfloat pieceWidth = 1.0 / 7.0;
    GLfloat texTop, texLeft, texBottom, texRight;
    if (piece->color == Black) {
        texTop = 0.0;
        texBottom = 0.5;
    } else { // White
        texTop = 0.5;
        texBottom = 1.0;
    }
    
    switch (piece->type) {
        case Rook:
            texLeft = 4.0 * pieceWidth;
            break;
        case Pawn:
            texLeft = 1.0 * pieceWidth;
            break;
        case Knight:
            texLeft = 2.0 * pieceWidth;
            break;
        case Biship:
            texLeft = 3.0 * pieceWidth;
            break;
        case King:
            texLeft = 5.0 * pieceWidth;
            break;
        case Queen:
            texLeft = 6.0 * pieceWidth;
            break;
    }
    
    texRight = texLeft + pieceWidth;
    
    GLfloat squareLength = 2.0 / 8.0;
    
    plotSquareInVertices(x, y, texTop, texLeft, texBottom, texRight, piece->vertexArrayIndex * 30, vertices);
}

void setupPieces(GLfloat *piecesVertices, struct Board *board, GLuint *vertexArrayId, GLuint *vertexBufferId) {
    GLuint _vertexArrayId;
    glGenVertexArrays(1, &_vertexArrayId);
    glBindVertexArray(_vertexArrayId);
    GLuint _vertexBufferId;
    glGenBuffers(1, &_vertexBufferId);
    
    for (int i = 0; i < 64; i++) {
        struct Piece *piece = board->squares[i];
        if (piece != NULL) {
            plotPieceInVertices(i, piece, piecesVertices);
        }
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 32 * 5 * 6, piecesVertices, GL_DYNAMIC_DRAW);

    *vertexBufferId  = _vertexBufferId;
    *vertexArrayId = _vertexArrayId;
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
    board->squares[3] = createPiece(Black, Queen, 3);
    board->squares[4] = createPiece(Black, King, 4);
    board->squares[5] = createPiece(Black, Biship, 5);
    board->squares[6] = createPiece(Black, Knight, 6);
    board->squares[7] = createPiece(Black, Rook, 7);
    board->squares[8] = createPiece(Black, Pawn, 8);
    board->squares[9] = createPiece(Black, Pawn, 9);
    board->squares[10] = createPiece(Black, Pawn, 10);
    board->squares[11] = createPiece(Black, Pawn, 11);
    board->squares[12] = createPiece(Black, Pawn, 12);
    board->squares[13] = createPiece(Black, Pawn, 13);
    board->squares[14] = createPiece(Black, Pawn, 14);
    board->squares[15] = createPiece(Black, Pawn, 15);
    
    board->squares[48] = createPiece(White, Pawn, 16);
    board->squares[49] = createPiece(White, Pawn, 17);
    board->squares[50] = createPiece(White, Pawn, 18);
    board->squares[51] = createPiece(White, Pawn, 19);
    board->squares[52] = createPiece(White, Pawn, 20);
    board->squares[53] = createPiece(White, Pawn, 21);
    board->squares[54] = createPiece(White, Pawn, 22);
    board->squares[55] = createPiece(White, Pawn, 23);
    board->squares[56] = createPiece(White, Rook, 24);
    board->squares[57] = createPiece(White, Knight, 25);
    board->squares[58] = createPiece(White, Biship, 26);
    board->squares[59] = createPiece(White, Queen, 27);
    board->squares[60] = createPiece(White, King, 28);
    board->squares[61] = createPiece(White, Biship, 29);
    board->squares[62] = createPiece(White, Knight, 30);
    board->squares[63] = createPiece(White, Rook, 31);
    
    
}

void cursorEnterCallback(GLFWwindow *window, int entered) {
    if (entered) {
    } else {
        // TODO: reset drag state
    }
}

void cursorPositionCallback(GLFWwindow *window, double posx, double posy) {
    // printf("mouse x = %f, y = %f\n", x, y);
    if (draggingSquare != -1) {
        struct Piece *piece = board.squares[draggingSquare];
        if (piece != NULL) {
            GLfloat squareLength = 2.0 / 8.0;
            // move the piece
            double x = 2.0 * posx / WINDOW_WIDTH - 0.5 * squareLength;
            double y = 2.0 - 2.0 * posy / WINDOW_HEIGHT - 0.5 * squareLength;
            // printf("Moving piece %d to x = %f, y = %f\n", draggingSquare, x, y);
            plotPieceXYInVertices(x, y, piece, piecesVertices);
            glBindVertexArray(piecesVertexArrayId);
            glBindBuffer(GL_ARRAY_BUFFER, piecesVertexBufferId);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 32 * 5 * 6, piecesVertices, GL_DYNAMIC_DRAW);
        }
    }
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int modifiers) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        double posx, posy;
        glfwGetCursorPos(window, &posx, &posy);
        int column = 8.0 * posx / WINDOW_WIDTH;
        int row = 8.0 * posy / WINDOW_HEIGHT;
        int boardPos = row * 8 + column;
        if (action == GLFW_PRESS) {
            printf("Start drag from x = %f, y = %f, row = %d, column = %d\n", posx, posy, row, column);
            draggingSquare = boardPos;
        } else { // action == GLFW_RELEASE
            // Find new position for piece
            struct Piece *piece = board.squares[draggingSquare];
            struct Piece *replacedPiece = board.squares[boardPos];
            if (replacedPiece != NULL && replacedPiece != piece) {
                // clean up
                plotPieceXYInVertices(-1.0, -1.0, replacedPiece, piecesVertices);
            }
            board.squares[draggingSquare] = NULL;
            board.squares[boardPos] = piece;
            plotPieceInVertices(boardPos, piece, piecesVertices);
            
            glBindVertexArray(piecesVertexArrayId);
            glBindBuffer(GL_ARRAY_BUFFER, piecesVertexBufferId);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 32 * 5 * 6, piecesVertices, GL_DYNAMIC_DRAW);
        
            draggingSquare = -1;
        }
    }
}

int appMainLoop() {
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
    
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetCursorEnterCallback(window, cursorEnterCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    
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
    
    boardVertexArrayId = setupBoard();
    glEnableVertexAttribArray(vertAttr);
    glVertexAttribPointer(vertAttr, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(vertTexCoordAttr);
    glVertexAttribPointer(vertTexCoordAttr, 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));
    
    GLuint numPieces = 1;
        
    setupPieces(piecesVertices, &board, &piecesVertexArrayId, &piecesVertexBufferId);
    
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
        glDrawArrays(GL_TRIANGLES, 0, 32 * 6);
        
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