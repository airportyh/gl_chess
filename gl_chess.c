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
#include "utarray.h"

#define WINDOW_WIDTH 720
#define WINDOW_HEIGHT 720
#define SQUARE_LENGTH 0.25
#define TIMELINE_THUMBNAIL_WIDTH 0.48
#define TIMELINE_GAP 0.02
#define ANIMATION_DURATION 40
#define TIME_MARKER_WIDTH 0.007

enum Piece {
    WPawn=0, WKnight, WBiship, WRook, WKing, WQueen,
    BPawn=8, BKnight, BBiship, BRook, BKing, BQueen,
    Blank=16
};

struct Board {
    enum Piece squares[64];
};

struct BoardView {
    GLfloat x;
    GLfloat y;
    GLfloat size;
};

struct GLSettings {
    GLuint boardProgram;
    GLuint piecesProgram;
    GLuint timeMarkerProgram;
    GLuint boardTextureId;
    GLuint boardTexUniformId;
    GLuint piecesTextureId;
    GLuint piecesTexUniformId;
    GLint  boardSizeUniform;
    GLint  boardTopLeftUniform;
    GLint  overrideIDUniform;
    GLint  overridePositionUniform;
    GLuint boardVertexArrayId;
    GLuint boardVertexBufferId;
    GLuint piecesVertexArrayId;
    GLuint piecesVertexBufferId;
    GLuint timeMarkerVertexArrayId;
    GLuint timeMarkerBufferId;
};

struct TimeMarkerAnimation {
    int targetTimestamp;
    GLfloat srcX;
    GLfloat dstX;
    int endTick; // animation is active if endTick is > 0
    int currentTick;
};

struct TimelineNode {
    UT_array *snapshots; // array of struct Board's
    UT_array *children;   // array of struct TimelineNode's
};

struct Board mainBoard;
struct BoardView mainBoardView;

UT_icd board_icd = { sizeof(struct Board), NULL, NULL, NULL };
UT_icd board_render_icd = { sizeof(struct BoardView), NULL, NULL, NULL };

struct GLSettings glSettings;
struct TimelineNode *timeline = NULL;
struct TimelineNode *currentTimelineBranch = NULL;
int currentTimestamp = 0; // TODO: rename to currentSnapshot?
GLfloat timeMarkerVertices[8];
int draggingTimeMarker = 0;
int draggingSquare = -1;
GLfloat draggingPieceX;
GLfloat draggingPieceY;
struct TimeMarkerAnimation timeMarkerAnimation;

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
        board->squares[i] = Blank;
    }
    
    board->squares[0] = BRook;
    board->squares[1] = BKnight;
    board->squares[2] = BBiship;
    board->squares[3] = BQueen;
    board->squares[4] = BKing;
    board->squares[5] = BBiship;
    board->squares[6] = BKnight;
    board->squares[7] = BRook;
    board->squares[8] = BPawn;
    board->squares[9] = BPawn;
    board->squares[10] = BPawn;
    board->squares[11] = BPawn;
    board->squares[12] = BPawn;
    board->squares[13] = BPawn;
    board->squares[14] = BPawn;
    board->squares[15] = BPawn;
    
    board->squares[48] = WPawn;
    board->squares[49] = WPawn;
    board->squares[50] = WPawn;
    board->squares[51] = WPawn;
    board->squares[52] = WPawn;
    board->squares[53] = WPawn;
    board->squares[54] = WPawn;
    board->squares[55] = WPawn;
    board->squares[56] = WRook;
    board->squares[57] = WKnight;
    board->squares[58] = WBiship;
    board->squares[59] = WQueen;
    board->squares[60] = WKing;
    board->squares[61] = WBiship;
    board->squares[62] = WKnight;
    board->squares[63] = WRook;
    
}

void printBoard(struct Board *board) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int i = row * 8 + col;
            printf("%d\t", board->squares[i]);
        }
        printf("\n");
    }
    printf("\n");
}

void initBuffers(
    struct GLSettings *glSettings) {
    GLuint piecesProgram = glSettings->piecesProgram;
    GLuint boardProgram = glSettings->boardProgram;
        
    // Init pieces vertex array and vertex buffer
    glGenVertexArrays(1, &glSettings->piecesVertexArrayId);
    glBindVertexArray(glSettings->piecesVertexArrayId);
    glGenBuffers(1, &glSettings->piecesVertexBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, glSettings->piecesVertexBufferId);
    
    GLint spriteTypeAttr = glGetAttribLocation(piecesProgram, "spriteType");
    glEnableVertexAttribArray(spriteTypeAttr);
    glVertexAttribIPointer(spriteTypeAttr, 1, GL_INT, sizeof(enum Piece), NULL);
    
    glSettings->boardSizeUniform = glGetUniformLocation(piecesProgram, "boardSize");
    glSettings->boardTopLeftUniform = glGetUniformLocation(piecesProgram, "boardTopLeft");
    glSettings->overrideIDUniform = glGetUniformLocation(piecesProgram, "overrideID");
    glSettings->overridePositionUniform = glGetUniformLocation(piecesProgram, "overridePosition");
    
    // Init board vertex array and vertex buffer
    glGenVertexArrays(1, &glSettings->boardVertexArrayId);
    glBindVertexArray(glSettings->boardVertexArrayId);
    glGenBuffers(1, &glSettings->boardVertexBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, glSettings->boardVertexBufferId);

    GLint boardVertexAttr = glGetAttribLocation(boardProgram, "vertex");
    GLint boardSizeAttr = glGetAttribLocation(boardProgram, "size");
    glEnableVertexAttribArray(boardVertexAttr);
    glVertexAttribPointer(boardVertexAttr, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(boardSizeAttr);
    glVertexAttribPointer(boardSizeAttr, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const GLvoid*)(2 * sizeof(GLfloat)));
    
    // Init time marker vertex array and vertex buffer
    glGenVertexArrays(1, &glSettings->timeMarkerVertexArrayId);
    glBindVertexArray(glSettings->timeMarkerVertexArrayId);
    glGenBuffers(1, &glSettings->timeMarkerBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, glSettings->timeMarkerBufferId);
    
    GLint posAttr = glGetAttribLocation(glSettings->timeMarkerProgram, "pos");
    glEnableVertexAttribArray(posAttr);
    glVertexAttribPointer(posAttr, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
}

void updatePiecesBuffer(struct GLSettings *glSettings, struct Board *board) {
    glBindBuffer(GL_ARRAY_BUFFER, glSettings->piecesVertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, 64 * sizeof(enum Piece), board, GL_STATIC_DRAW);
}

void updateBoardBuffer(struct GLSettings *glSettings, struct BoardView *boardView) {
    glBindBuffer(GL_ARRAY_BUFFER, glSettings->boardVertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(GLfloat), boardView, GL_STATIC_DRAW);
}

int compileProgram(char *vertexShaderFile, char *geometryShaderFile, char *fragmentShaderFile) {
    GLuint program = glCreateProgram();
    
    char *vertexShaderSource;
    CALL(readFile(vertexShaderFile, &vertexShaderSource));
    GLuint vertexShaderId;
    CALL(compileShader(vertexShaderSource, GL_VERTEX_SHADER, &vertexShaderId));
    glAttachShader(program, vertexShaderId);
    
    if (geometryShaderFile) { // Geometry shader is optional
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

void initGLSettings(struct GLSettings *glSettings) {
    glSettings->piecesProgram = compileProgram("shaders/piece_vertex_shader.glsl", "shaders/piece_geometry_shader.glsl", "shaders/common_fragment_shader.glsl");
    glSettings->boardProgram = compileProgram("shaders/board_vertex_shader.glsl", "shaders/board_geometry_shader.glsl", "shaders/common_fragment_shader.glsl");
    glSettings->timeMarkerProgram = compileProgram("shaders/time_marker_vertex_shader.glsl", NULL, "shaders/time_marker_fragment_shader.glsl");
    glSettings->piecesTextureId = loadTexture("sprite.png");
    glSettings->piecesTexUniformId = glGetUniformLocation(glSettings->piecesProgram, "tex");
    glSettings->boardTextureId = loadTexture("board.png");
    glSettings->boardTexUniformId = glGetUniformLocation(glSettings->boardProgram, "tex");
}

void renderBoard(
    struct GLSettings *glSettings, struct BoardView *boardView,
    GLint overrideId, GLfloat overrideX, GLfloat overrideY
) {
    glUseProgram(glSettings->boardProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glSettings->boardTextureId);
    glUniform1i(glSettings->boardTexUniformId, 0);
    glBindVertexArray(glSettings->boardVertexArrayId);
    glDrawArrays(GL_POINTS, 0, 1);
    
    glUseProgram(glSettings->piecesProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glSettings->piecesTextureId);
    glUniform1i(glSettings->piecesTexUniformId, 0);
    glUniform1f(glSettings->boardSizeUniform, boardView->size);
    glUniform2f(glSettings->boardTopLeftUniform, boardView->x, boardView->y);
    
    glUniform1i(glSettings->overrideIDUniform, overrideId);
    glUniform2f(glSettings->overridePositionUniform, overrideX, overrideY);
    
    glBindVertexArray(glSettings->piecesVertexArrayId);
    glDrawArrays(GL_POINTS, 0, 64);
}

void addToTimeline(struct Board *board) {
    int timelineLength = utarray_len(timeline->snapshots);
    if (timelineLength == 0 || (currentTimestamp == timelineLength - 1)) {
        // printf("Pushing to end of timeline\n");
        utarray_push_back(timeline->snapshots, board);
        currentTimestamp = utarray_len(timeline->snapshots) - 1;
    } else {
        printf("Forking timeline. currentTimestamp = %d, timelineLength = %d\n", currentTimestamp, timelineLength);
        // Create an alternate timeline
        struct Board *currentBoard = utarray_eltptr(timeline->snapshots, currentTimestamp);    
    }
}

void renderTimeline() {
    int length = utarray_len(timeline->snapshots);
    if (length <= 1) {
        return;
    }
    
    int numThumbnails = 4.0 / (TIMELINE_THUMBNAIL_WIDTH + TIMELINE_GAP);
    for (int i = 0; i < numThumbnails; i++) {
        int index = floor((float)i * length / numThumbnails);
        struct Board *board = utarray_eltptr(timeline->snapshots, index);
        struct BoardView boardView;
        boardView.x = (TIMELINE_THUMBNAIL_WIDTH + TIMELINE_GAP) * i - 2 + 0.01;
        boardView.y = -2;
        boardView.size = TIMELINE_THUMBNAIL_WIDTH;
        updateBoardBuffer(&glSettings, &boardView);
        updatePiecesBuffer(&glSettings, board);
        renderBoard(&glSettings, &boardView, -1, 0, 0);
    }
}


void renderTimeMarker() {
    int timelineLength = utarray_len(timeline->snapshots);
    GLfloat timeMarkerGap = 0.0001;
    GLfloat x;
    if (timelineLength <= 1) {
        return;
    }
    
    if (timeMarkerAnimation.endTick > 0) {
        // render tween state
        GLfloat animationPercent = (float)timeMarkerAnimation.currentTick / (float)timeMarkerAnimation.endTick;
        x = (1.0 - animationPercent) * timeMarkerAnimation.srcX + animationPercent * timeMarkerAnimation.dstX;
        // printf("animationPercent = %f, x = %f\n", animationPercent, x);
        
    } else {
        x = (2 - 2 * timeMarkerGap) * (((float)currentTimestamp) / (float)(timelineLength - 1)) - 1 + timeMarkerGap;
    }
    
    x -= 0.005;
    glUseProgram(glSettings.timeMarkerProgram);
    timeMarkerVertices[0] = x;
    timeMarkerVertices[1] = -0.76;
    timeMarkerVertices[2] = x + TIME_MARKER_WIDTH;
    timeMarkerVertices[3] = -0.76;
    timeMarkerVertices[4] = x;
    timeMarkerVertices[5] = -1.2;
    timeMarkerVertices[6] = x + TIME_MARKER_WIDTH;
    timeMarkerVertices[7] = -1.2;
    
    glBindBuffer(GL_ARRAY_BUFFER, glSettings.timeMarkerBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(timeMarkerVertices), timeMarkerVertices, GL_STATIC_DRAW);
    glBindVertexArray(glSettings.timeMarkerVertexArrayId);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void updateMainBoard() {
    memcpy(&mainBoard, utarray_eltptr(timeline->snapshots, currentTimestamp), sizeof(struct Board));
}

void animateTimeMarkerToTimestamp(int timestamp) {
    if (timeMarkerAnimation.targetTimestamp == timestamp) {
        return;
    }
    GLfloat srcX;
    int timelineLength = utarray_len(timeline->snapshots);
    GLfloat timeMarkerGap = 0.0001;
    GLfloat dstX = (2 - 2 * timeMarkerGap) * (((float)timestamp) / (float)(timelineLength - 1)) - 1 + timeMarkerGap;
    if (timeMarkerAnimation.endTick != 0) {
        GLfloat animationPercent = (float)timeMarkerAnimation.currentTick / (float)timeMarkerAnimation.endTick;
        srcX = (1.0 - animationPercent) * timeMarkerAnimation.srcX + animationPercent * timeMarkerAnimation.dstX;
    } else {
        srcX = (2 - 2 * timeMarkerGap) * (((float)currentTimestamp) / (float)(timelineLength - 1)) - 1 + timeMarkerGap;
    }
    
    timeMarkerAnimation.targetTimestamp = timestamp;
    timeMarkerAnimation.endTick = ANIMATION_DURATION;
    timeMarkerAnimation.currentTick = 0;
    timeMarkerAnimation.srcX = srcX;
    timeMarkerAnimation.dstX = dstX;
    currentTimestamp = timestamp;
    updateMainBoard();
    
}

void updateTimeMarkerPosition(double posx) {
    int timelineLength = utarray_len(timeline->snapshots);
    GLfloat timestampPercent = posx / WINDOW_WIDTH;
    int newCurrentTimestamp = round((timelineLength - 1) * timestampPercent);
    if (newCurrentTimestamp != currentTimestamp) {
        animateTimeMarkerToTimestamp(newCurrentTimestamp);
    }
}

void updateTimeMarkerState() {
    if (timeMarkerAnimation.endTick > 0) {
        // Update animation
        if (timeMarkerAnimation.currentTick >= timeMarkerAnimation.endTick) {
            // Animation complete
            // printf("Animation complete\n");
            timeMarkerAnimation.endTick = 0;
            currentTimestamp = timeMarkerAnimation.targetTimestamp;
            updateMainBoard();
        } else {
            timeMarkerAnimation.currentTick++;
        }
    }
}

void initTimeline() {
    timeline = malloc(sizeof(struct TimelineNode));
    utarray_new(timeline->snapshots, &board_icd);
}

void commitMove(int destPos, int srcPos) {
    enum Piece piece = mainBoard.squares[srcPos];
    if (destPos != srcPos) {
        mainBoard.squares[destPos] = piece;
        mainBoard.squares[srcPos] = Blank;
    }
    piece = mainBoard.squares[destPos];
    updatePiecesBuffer(&glSettings, &mainBoard);
    if (destPos != srcPos) {
        addToTimeline(&mainBoard);
    }
}

void updateDraggingPiecePosition(double posx, double posy) {
    GLfloat squareLength = 2.0 / 8.0;
    draggingPieceX = (posx - WINDOW_WIDTH / 2) * 4 / WINDOW_WIDTH - 0.5 * squareLength - mainBoardView.x;
    draggingPieceY = - (posy - WINDOW_HEIGHT / 2) * 4 / WINDOW_HEIGHT - 0.5 * squareLength - mainBoardView.y;
}

/*

<Input Event Handlers>

*/

void cursorEnterCallback(GLFWwindow *window, int entered) {
    if (entered) {
    } else {
        // TODO: reset drag state
    }
}

void cursorPositionCallback(GLFWwindow *window, double posx, double posy) {
    // printf("mouse x = %f, y = %f\n", x, y);
    if (draggingSquare != -1) {
        updateDraggingPiecePosition(posx, posy);
    } else if (posy >= 7 * WINDOW_HEIGHT / 8 && draggingTimeMarker) {
        updateTimeMarkerPosition(posx);
    }
}

void handleMainBoardMouseClick(int button, int action, double posx, double posy) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        int column = 4 + 8.0 * (posx - WINDOW_WIDTH / 2) / (WINDOW_WIDTH / 2);
        int row = 4 + 8.0 * (posy - WINDOW_HEIGHT / 2) / (WINDOW_HEIGHT / 2) ;
        int boardPos = row * 8 + column;
        if (action == GLFW_PRESS) {
            // printf("Start drag from row = %d, column = %d, boardPos = %d\n", row, column, boardPos);
            draggingSquare = boardPos;
            updateDraggingPiecePosition(posx, posy);
        } else { // action == GLFW_RELEASE
            commitMove(boardPos, draggingSquare);
            draggingSquare = -1;
        }
    }
}

void handleTimelineMouseClick(int button, int action, double posx, double posy) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_RELEASE) {
            updateTimeMarkerPosition(posx);
            draggingTimeMarker = 0;
        } else if (action == GLFW_PRESS) {
            updateTimeMarkerPosition(posx);
            draggingTimeMarker = 1;
        }
    }
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int modifiers) {
    double posx, posy;
    glfwGetCursorPos(window, &posx, &posy);
    // printf("x = %f, y = %f\n", posx, posy);
    if (posx >= WINDOW_WIDTH / 4 && posx <= 3 * WINDOW_WIDTH / 4 && posy >= WINDOW_HEIGHT / 4 && posy <= 3 * WINDOW_HEIGHT / 4) {
        handleMainBoardMouseClick(button, action, posx, posy);
    } else if (posy >= 7 * WINDOW_HEIGHT / 8) {
        handleTimelineMouseClick(button, action, posx, posy);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
        int targetTimestamp = currentTimestamp - 1;
        if (targetTimestamp < 0) {
            targetTimestamp = utarray_len(timeline->snapshots) - 1;
        }
        animateTimeMarkerToTimestamp(targetTimestamp);
    } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        int targetTimestamp = currentTimestamp + 1;
        if (targetTimestamp >= utarray_len(timeline->snapshots)) {
            targetTimestamp = 0;
        }
        animateTimeMarkerToTimestamp(targetTimestamp);
    }
}

/*

</Input Event Handlers>

*/

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
    glfwSetKeyCallback(window, keyCallback);
    
    if (glewInit() != GLEW_OK) {
        printf("glewInit failed.\n");
        return 1;
    }
    
    displayGLVersions();
    
    initGLSettings(&glSettings);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.05, 0.15, 0.05, 1.0);
    initBuffers(&glSettings);
    timeMarkerAnimation.endTick = 0;
    
    initBoard(&mainBoard);
    
    mainBoardView.x = -1;
    mainBoardView.y = -1;
    mainBoardView.size = 2;
    
    initTimeline();
    
    addToTimeline(&mainBoard);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        updateBoardBuffer(&glSettings, &mainBoardView);
        updatePiecesBuffer(&glSettings, &mainBoard);
        // printf("boardView.x = %f, boardView.y = %f\n", mainBoardView.x, mainBoardView.y);
        
        renderBoard(&glSettings, &mainBoardView, draggingSquare, draggingPieceX, draggingPieceY);
        renderTimeline();
        updateTimeMarkerState();
        renderTimeMarker();
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