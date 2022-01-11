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
    Black, White, None
};

enum SpriteType {
    WPawn=0, WKnight, WBiship, WRook, WKing, WQueen,
    BPawn=8, BKnight, BBiship, BRook, BKing, BQueen,
    WSquare=16, BSquare
};

struct Piece {
    enum SpriteType type;
    int vertexArrayIndex;
};

struct Board {
    struct Piece *squares[64];
};

struct SpriteRender {
    GLfloat x;
    GLfloat y;
    enum SpriteType spriteType;
    GLfloat size;
};

struct BoardRender {
    GLfloat x;
    GLfloat y;
    GLfloat size;
    struct SpriteRender pieceVertices[32];
    GLuint boardVertexArrayId;
    GLuint boardVertexBufferId;
    GLuint piecesVertexArrayId;
    GLuint piecesVertexBufferId;
};

struct BoardData {
    struct Board board;
    struct BoardRender boardRender;
};

// A timeline is actually a tree, because a timeline can be forked.
// TimelineListNode then is a linked list of alternate outcomes for the next step.
struct TimelineNode {
    struct BoardData *state;
    struct TimelineListNode *children;
};

struct TimelineListNode {
    struct TimelineNode *data;
    struct TimelineListNode *next;
};

struct GLSettings {
    GLuint boardProgram;
    GLuint piecesProgram;
    GLuint boardTextureId;
    GLuint boardTexUniformId;
    GLuint piecesTextureId;
    GLuint piecesTexUniformId;
};

struct BoardData *mainBoard = NULL;
struct TimelineNode *timeline = NULL;
struct TimelineNode *currTimelineNode = NULL;
struct GLSettings glSettings;

static int draggingSquare = -1;

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
        } else if (shaderType == GL_GEOMETRY_SHADER) {
            printf("Compile geometry shader failed.\n");
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

struct Piece *createPiece(enum SpriteType type, int vertexArrayIndex) {
    struct Piece *piece = malloc(sizeof(struct Piece));
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
    
    board->squares[0] = createPiece(BRook, 0);
    board->squares[1] = createPiece(BKnight, 1);
    board->squares[2] = createPiece(BBiship, 2);
    board->squares[3] = createPiece(BQueen, 3);
    board->squares[4] = createPiece(BKing, 4);
    board->squares[5] = createPiece(BBiship, 5);
    board->squares[6] = createPiece(BKnight, 6);
    board->squares[7] = createPiece(BRook, 7);
    board->squares[8] = createPiece(BPawn, 8);
    board->squares[9] = createPiece(BPawn, 9);
    board->squares[10] = createPiece(BPawn, 10);
    board->squares[11] = createPiece(BPawn, 11);
    board->squares[12] = createPiece(BPawn, 12);
    board->squares[13] = createPiece(BPawn, 13);
    board->squares[14] = createPiece(BPawn, 14);
    board->squares[15] = createPiece(BPawn, 15);
    
    board->squares[48] = createPiece(WPawn, 16);
    board->squares[49] = createPiece(WPawn, 17);
    board->squares[50] = createPiece(WPawn, 18);
    board->squares[51] = createPiece(WPawn, 19);
    board->squares[52] = createPiece(WPawn, 20);
    board->squares[53] = createPiece(WPawn, 21);
    board->squares[54] = createPiece(WPawn, 22);
    board->squares[55] = createPiece(WPawn, 23);
    board->squares[56] = createPiece(WRook, 24);
    board->squares[57] = createPiece(WKnight, 25);
    board->squares[58] = createPiece(WBiship, 26);
    board->squares[59] = createPiece(WQueen, 27);
    board->squares[60] = createPiece(WKing, 28);
    board->squares[61] = createPiece(WBiship, 29);
    board->squares[62] = createPiece(WKnight, 30);
    board->squares[63] = createPiece(WRook, 31);
    
}

void initBuffers(
    struct GLSettings *glSettings,
    struct BoardRender *boardRender) {
    GLuint piecesProgram = glSettings->piecesProgram;
    GLuint boardProgram = glSettings->boardProgram;
        
    // Init pieces vertex array and vertex buffer
    glGenVertexArrays(1, &boardRender->piecesVertexArrayId);
    glBindVertexArray(boardRender->piecesVertexArrayId);
    printf("Generated vertex array for pieces %d\n", boardRender->piecesVertexArrayId);
    glGenBuffers(1, &boardRender->piecesVertexBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, boardRender->piecesVertexBufferId);
    printf("Generated vertex buffer for pieces %d\n", boardRender->piecesVertexBufferId);

    GLint vertexAttr = glGetAttribLocation(piecesProgram, "vertex");
    GLint spriteTypeAttr = glGetAttribLocation(piecesProgram, "spriteType");
    GLint sizeAttr = glGetAttribLocation(piecesProgram, "size");

    glEnableVertexAttribArray(vertexAttr);
    glVertexAttribPointer(vertexAttr, 2, GL_FLOAT, GL_FALSE, sizeof(struct SpriteRender), NULL);
    glEnableVertexAttribArray(spriteTypeAttr);
    glVertexAttribIPointer(spriteTypeAttr, 1, GL_INT, sizeof(struct SpriteRender), (const GLvoid*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(sizeAttr);
    glVertexAttribPointer(sizeAttr, 1, GL_FLOAT, GL_FALSE, sizeof(struct SpriteRender), (const GLvoid*)(2 * sizeof(GLfloat) + sizeof(GLint)));

    // Init board vertex array and vertex buffer
    glGenVertexArrays(1, &boardRender->boardVertexArrayId);
    glBindVertexArray(boardRender->boardVertexArrayId);
    glGenBuffers(1, &boardRender->boardVertexBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, boardRender->boardVertexBufferId);

    GLint boardVertexAttr = glGetAttribLocation(boardProgram, "vertex");
    GLint boardSizeAttr = glGetAttribLocation(boardProgram, "size");

    glEnableVertexAttribArray(boardVertexAttr);
    glVertexAttribPointer(boardVertexAttr, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(boardSizeAttr);
    glVertexAttribPointer(boardSizeAttr, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const GLvoid*)(2 * sizeof(GLfloat)));
        
}

void updatePiecesBuffer(struct BoardRender *boardRender) {
    glBindBuffer(GL_ARRAY_BUFFER, mainBoard->boardRender.piecesVertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, 32 * sizeof(struct SpriteRender), boardRender->pieceVertices, GL_STATIC_DRAW);
    
}

void updateBoardBuffer(struct BoardRender *boardRender) {
    glBindBuffer(GL_ARRAY_BUFFER, mainBoard->boardRender.boardVertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(GLfloat), boardRender, GL_STATIC_DRAW);
}

int compileProgram(char *vertexShaderFile, char *geometryShaderFile, char *fragmentShaderFile) {
    GLuint program = glCreateProgram();
    
    char *vertexShaderSource;
    CALL(readFile(vertexShaderFile, &vertexShaderSource));
    GLuint vertexShaderId;
    CALL(compileShader(vertexShaderSource, GL_VERTEX_SHADER, &vertexShaderId));
    glAttachShader(program, vertexShaderId);
    
    if (geometryShaderFile) {
        char *geometryShaderSource;
        CALL(readFile(geometryShaderFile, &geometryShaderSource));
        GLuint geometryShaderId;
        CALL(compileShader(geometryShaderSource, GL_GEOMETRY_SHADER, &geometryShaderId));
        glAttachShader(program, geometryShaderId);
    }
    
    char *fragmentShaderSource;
    CALL(readFile(fragmentShaderFile, &fragmentShaderSource));
    GLuint fragmentShaderId;
    CALL(compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER, &fragmentShaderId));
    glAttachShader(program, fragmentShaderId);
    
    glLinkProgram(program);
    
    return program;
}

int loadTexture(char *imageFile) {
    GLuint textureId;
    int imageWidth, imageHeight, imageChannels;
    unsigned char* imagePixels;
    imagePixels = stbi_load(imageFile, &imageWidth, &imageHeight, &imageChannels, 0);
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
    return textureId;
}

void populatePieceVertices(struct BoardData *boardData)
// Assumes boardRender's x, y, and size are pre-specified
{
    struct Board *board = &boardData->board;
    struct BoardRender *boardRender = &boardData->boardRender;
    GLfloat left = boardRender->x;
    GLfloat top = boardRender->y;
    GLfloat squareLength = boardRender->size / 8;
    for (int i = 0; i < 64; i++) {
        struct Piece *piece = board->squares[i];
        if (piece != NULL) {
            GLfloat x = left + squareLength * (i % 8);
            GLfloat y = top + squareLength * (7 - floor(i / 8));
            boardRender->pieceVertices[piece->vertexArrayIndex].x = x;
            boardRender->pieceVertices[piece->vertexArrayIndex].y = y;
            boardRender->pieceVertices[piece->vertexArrayIndex].spriteType = piece->type;
            boardRender->pieceVertices[piece->vertexArrayIndex].size = squareLength;
        }
    }
}

void initGLSettings(struct GLSettings *glSettings) {
    glSettings->piecesProgram = compileProgram("vertex_shader2.glsl", "geometry_shader2.glsl", "fragment_shader2.glsl");
    glSettings->boardProgram = compileProgram("board_vertex_shader.glsl", "board_geometry_shader.glsl", "fragment_shader2.glsl");
    glSettings->piecesTextureId = loadTexture("sprite.png");
    glSettings->piecesTexUniformId = glGetUniformLocation(glSettings->piecesProgram, "tex");
    glSettings->boardTextureId = loadTexture("board.png");
    glSettings->boardTexUniformId = glGetUniformLocation(glSettings->boardProgram, "tex");
}

void renderBoard(struct BoardData *boardData, struct GLSettings *glSettings) {
    glUseProgram(glSettings->boardProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glSettings->boardTextureId);
    glUniform1i(glSettings->boardTexUniformId, 0);
    glBindVertexArray(boardData->boardRender.boardVertexArrayId);
    glDrawArrays(GL_POINTS, 0, 1);
    
    glUseProgram(glSettings->piecesProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glSettings->piecesTextureId);
    glUniform1i(glSettings->piecesTexUniformId, 0);
    glBindVertexArray(boardData->boardRender.piecesVertexArrayId);
    glDrawArrays(GL_POINTS, 0, 32);
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
        struct Piece *piece = mainBoard->board.squares[draggingSquare];
        if (piece != NULL) {
            GLfloat squareLength = 2.0 / 8.0;
            // move the piece
            double x = (posx - WINDOW_WIDTH / 2) * 4 / WINDOW_WIDTH - 0.5 * squareLength;
            double y = - (posy - WINDOW_HEIGHT / 2) * 4 / WINDOW_HEIGHT - 0.5 * squareLength;
            // printf("Moving piece %d to x = %f, y = %f\n", draggingSquare, x, y);
            struct SpriteRender *sr = &mainBoard->boardRender.pieceVertices[piece->vertexArrayIndex];
            sr->x = x;
            sr->y = y;
            updatePiecesBuffer(&mainBoard->boardRender);
        }
    }
}

void printTimeline(struct TimelineNode *timeline, int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("printTimeline\n");
    struct TimelineListNode *children = timeline->children;
    while (children != NULL) {
        printTimeline(children->data, level + 1);
        children = children->next;
    }
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int modifiers) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        double posx, posy;
        glfwGetCursorPos(window, &posx, &posy);
        printf("x = %f, y = %f\n", posx, posy);
        int column = 4 + 8.0 * (posx - WINDOW_WIDTH / 2) / (WINDOW_WIDTH / 2);
        int row = 4 + 8.0 * (posy - WINDOW_HEIGHT / 2) / (WINDOW_HEIGHT / 2) ;
        int boardPos = row * 8 + column;
        if (action == GLFW_PRESS) {
            printf("Start drag from row = %d, column = %d, boardPos = %d\n", row, column, boardPos);
            draggingSquare = boardPos;
        } else { // action == GLFW_RELEASE
            // Find new position for piece
            
            struct Piece *piece = mainBoard->board.squares[draggingSquare];
            struct Piece *replacedPiece = mainBoard->board.squares[boardPos];
            if (replacedPiece != NULL && replacedPiece != piece) {
                // clean up
                struct SpriteRender *sr = &mainBoard->boardRender.pieceVertices[replacedPiece->vertexArrayIndex];
                sr->x = -1.0;
                sr->y = -1.0;
            }
            mainBoard->board.squares[draggingSquare] = NULL;
            mainBoard->board.squares[boardPos] = piece;
            
            struct SpriteRender *sr = &mainBoard->boardRender.pieceVertices[piece->vertexArrayIndex];
            GLfloat squareLength = mainBoard->boardRender.size / 8;
            GLfloat x = mainBoard->boardRender.x + squareLength * (boardPos % 8);
            GLfloat y = mainBoard->boardRender.y + squareLength * (7 - floor(boardPos / 8));
            
            // Make copy of board and add to timeline before making the move
            printf("Adding new timestamp to timeline\n");
            
            
            struct BoardData *newBoardData = malloc(sizeof(struct BoardData));
            struct TimelineNode *newTimelineNode = malloc(sizeof(struct TimelineNode));
            newTimelineNode->state = newBoardData;
            newTimelineNode->children = NULL;
            memcpy(&newBoardData->board, &mainBoard->board, sizeof(struct Board));
            
            if (currTimelineNode != NULL) {
                if (currTimelineNode->children == NULL) {
                    struct TimelineListNode *children = malloc(sizeof(struct TimelineListNode));
                    children->data = newTimelineNode;
                    children->next = NULL;
                    currTimelineNode->children = children;
                } else {
                    struct TimelineListNode *node = currTimelineNode->children;
                    while (node->next != NULL) {
                        node = node->next;
                    }
                    node->next = malloc(sizeof(struct TimelineListNode));
                    node->next->data = newTimelineNode;
                    node->next->next = NULL;
                }
            }
            
            if (timeline == NULL) {
                timeline = newTimelineNode;
            }
            currTimelineNode = newTimelineNode;
            
            currTimelineNode->state->boardRender.x = -2;
            currTimelineNode->state->boardRender.y = -2;
            currTimelineNode->state->boardRender.size = 0.5;
            
            // populatePieceVertices(currTimelineNode->state);
            // initBuffers2(&glSettings, &currTimelineNode->state->boardRender);
            // updateBoardBuffer(&currTimelineNode->state->boardRender);
            // updatePiecesBuffer(&currTimelineNode->state->boardRender);
            
            printf("Added new timestamp to timeline\n");
            printTimeline(timeline, 0);
            
            sr->x = x;
            sr->y = y;
            
            updatePiecesBuffer(&mainBoard->boardRender);
            
            draggingSquare = -1;
        }
    }
}

int appMainLoop() {
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
    
    initGLSettings(&glSettings);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    mainBoard = malloc(sizeof(struct BoardData));
    
    // Init board 1
    initBoard(&mainBoard->board);
    
    mainBoard->boardRender.x = -1;
    mainBoard->boardRender.y = -1;
    mainBoard->boardRender.size = 2;
    
    populatePieceVertices(mainBoard);
    initBuffers(&glSettings, &mainBoard->boardRender);
    updateBoardBuffer(&mainBoard->boardRender);
    updatePiecesBuffer(&mainBoard->boardRender);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        renderBoard(mainBoard, &glSettings);
        
        if (timeline != NULL) {
            renderBoard(timeline->state, &glSettings);
        }
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