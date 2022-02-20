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
#define TIMELINE_THUMBNAIL_GAP 0.02
#define TIMELINE_GAP 0.1
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

struct TimelineNode {
    struct TimelineNode *parent;
    UT_array *snapshots; // array of struct Board's
    UT_array *children;   // array of struct TimelineNode's
};

struct TimelineViewNode {
    GLfloat x;
    GLfloat y;
    GLfloat width;
    GLfloat height;
    struct TimelineNode *timeline;
    UT_array *children; // array of struct TimelineViewNode's
};

struct GLSettings {
    GLuint boardProgram;
    GLuint piecesProgram;
    GLuint timeMarkerProgram;
    GLuint boardTextureId;
    GLuint boardTexUniformId;
    GLuint boardPerspectiveUniformId;
    GLuint piecesTextureId;
    GLuint piecesTexUniformId;
    GLuint piecesPerspectiveUniformId;
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

struct Board mainBoard;
struct BoardView mainBoardView;

UT_icd board_icd = { sizeof(struct Board), NULL, NULL, NULL };
UT_icd timeline_view_icd = { sizeof(struct TimelineViewNode), NULL, NULL };

struct GLSettings glSettings;
struct TimelineNode *rootTimeline = NULL;
struct TimelineNode *currTimeline = NULL;
struct TimelineViewNode timelineView;
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

GLfloat max(GLfloat a, GLfloat b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

GLfloat min(GLfloat a, GLfloat b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
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

struct TimelineNode *newTimeline(struct TimelineNode *parent) {
    struct TimelineNode *tl = malloc(sizeof(struct TimelineNode));
    tl->parent = parent;
    utarray_new(tl->snapshots, &board_icd);
    utarray_new(tl->children, &timeline_view_icd);
    return tl;
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
    glSettings->piecesProgram = compileProgram(
        "shaders/piece_vertex_shader.glsl", 
        "shaders/piece_geometry_shader.glsl", 
        "shaders/common_fragment_shader.glsl"
    );
    glSettings->boardProgram = compileProgram(
        "shaders/board_vertex_shader.glsl", 
        "shaders/board_geometry_shader.glsl", 
        "shaders/common_fragment_shader.glsl"
    );
    glSettings->timeMarkerProgram = compileProgram("shaders/time_marker_vertex_shader.glsl", NULL, "shaders/time_marker_fragment_shader.glsl");
    glSettings->piecesTextureId = loadTexture("sprite.png");
    glSettings->piecesTexUniformId = glGetUniformLocation(glSettings->piecesProgram, "tex");
    glSettings->piecesPerspectiveUniformId = glGetUniformLocation(glSettings->piecesProgram, "perspective");
    glSettings->boardTextureId = loadTexture("board.png");
    glSettings->boardTexUniformId = glGetUniformLocation(glSettings->boardProgram, "tex");
    glSettings->boardPerspectiveUniformId = glGetUniformLocation(glSettings->boardProgram, "perspective");
}

void renderBoard(
    struct GLSettings *glSettings, struct BoardView *boardView,
    GLint overrideId, GLfloat overrideX, GLfloat overrideY
) {
    GLfloat a = 2.0 / WINDOW_WIDTH;
    GLfloat b = -2.0 / WINDOW_HEIGHT;
    GLfloat c = -1;
    GLfloat d = 1;
    const GLfloat perspectiveMatrix[16] = {
        a, 0, 0, c, 
        0, b, 0, d,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    glUseProgram(glSettings->boardProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glSettings->boardTextureId);
    glUniform1i(glSettings->boardTexUniformId, 0);
    glUniformMatrix4fv(glSettings->boardPerspectiveUniformId, 1, GL_TRUE, perspectiveMatrix);
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
    glUniformMatrix4fv(glSettings->piecesPerspectiveUniformId, 1, GL_TRUE, perspectiveMatrix);
    
    glBindVertexArray(glSettings->piecesVertexArrayId);
    glDrawArrays(GL_POINTS, 0, 64);
}

void printIndent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf(" ");
    }
}

void printTimeline(struct TimelineNode *timeline, int indent) {
    if (timeline == NULL) {
        return;
    }
    // printIndent(indent);
    // printf("printTimeline\n");
    // printIndent(indent);
    // printf("timeline=%d, snapshots=%d\n", (int)timeline, (int)timeline->snapshots);
    int timelineLen = utarray_len(timeline->snapshots);
    // printIndent(indent);
    // printf("got timelineLen\n");
    printIndent(indent);
    for (int i = 0; i < timelineLen; i++) {
        printf("o");
    }
    int numChildren = utarray_len(timeline->children);
    if (numChildren > 0) {
        // printIndent(indent);
        // printf("process children\n");
        for (int i = 0; i < numChildren; i++) {
            struct TimelineNode *child = utarray_eltptr(timeline->children, i);
            if (i == 0) {
                printf("  ");
                printTimeline(child, 0);
            } else {
                printTimeline(child, indent + timelineLen + 2);
            }
        }    
    } else {
        // printIndent(indent);
        // printf("end printTimeline\n");
        printf("\n");
    }
}

void printTimelineView(struct TimelineViewNode *timelineView, int level) {
    printIndent(level);
    printf("TLV(x=%f, y=%f, width=%f, height=%f, count=%d)\n", 
        timelineView->x, timelineView->y,
        timelineView->width, timelineView->height,
        utarray_len(timelineView->timeline->snapshots)
    );
    if (timelineView->children == NULL) {
        return;
    }
    int numChildren = utarray_len(timelineView->children);
    for (int i = 0; i < numChildren; i++) {
        printTimelineView(utarray_eltptr(timelineView->children, i), level + 1);
    }
}

int getTotalTimelineLength(struct TimelineNode *timeline) {
    if (timeline == NULL) {
        return 0;
    }
    if (timeline->children == NULL) {
        return 1;
    }
    int numChildren = utarray_len(timeline->children);
    int maxChildLength = 0;
    for (int i = 0; i < numChildren; i++) {
        struct TimelineNode *child = utarray_eltptr(timeline->children, i);
        int childLength = getTotalTimelineLength(child);
        if (childLength > maxChildLength) {
            maxChildLength = childLength;
        }
    }
    int length = utarray_len(timeline->snapshots);
    return length + maxChildLength;
}

int getTotalTimelineHeight(struct TimelineNode *timeline) {
    if (timeline == NULL) {
        return 0;
    }
    if (timeline->children == NULL) {
        return 1;
    }
    int numChildren = utarray_len(timeline->children);
    if (numChildren == 0) {
        return 1;
    }
    int sumChildrenHeight = 0;
    for (int i = 0; i < numChildren; i++) {
        struct TimelineNode *child = utarray_eltptr(timeline->children, i);
        int childrenHeight = getTotalTimelineHeight(child);
        sumChildrenHeight += childrenHeight;
    }
    return sumChildrenHeight;
}

void freeTimelineView(struct TimelineViewNode *node) {
    if (node == NULL) {
        return;
    }
    
    if (node->children != NULL) {
        int numChildren = utarray_len(node->children);
        for (int i = 0; i < numChildren; i++) {
            struct TimelineViewNode *child = utarray_eltptr(node->children, i);
            freeTimelineView(child);
        }
        utarray_free(node->children);
    }
}

void doLayoutTimeline(
    struct TimelineNode *timeline,
    GLfloat offsetx, GLfloat offsety,
    int totalLength, int totalHeight,
    GLfloat width, GLfloat heightPerLine,
    struct TimelineViewNode *timelineView,
    int level
) {
    // printIndent(level);
    if (timeline == NULL) {
        // printIndent(level);
        // printf("Early return\n");
        return;
    }
    timelineView->timeline = timeline;
    timelineView->children = NULL;
    timelineView->x = offsetx;
    int timelineLength = utarray_len(timeline->snapshots);
    timelineView->width = ((float)timelineLength / (float)totalLength) * width;
    int length = utarray_len(timeline->snapshots);
    GLfloat widthPerThumbnail = min(heightPerLine, max(0.25, timelineView->width / length));
    GLfloat thumbnailWidth = widthPerThumbnail;
    
    timelineView->height = thumbnailWidth;
    timelineView->y = offsety;
    
    // printIndent(level);
    // printf("has children array\n");
    int numChildren = utarray_len(timeline->children);
    if (numChildren == 0) {
        // printIndent(level);
        // printf("has no children\n");
        return;
    }
    utarray_new(timelineView->children, &timeline_view_icd);
    // printf("Allocated timelineView->children %u\n", (unsigned)timelineView->children);
    for (int i = 0; i < numChildren; i++) {
        struct TimelineNode *child = utarray_eltptr(timeline->children, i);
        struct TimelineViewNode childView;
        doLayoutTimeline(
            child,
            offsetx + timelineView->width, offsety + i * thumbnailWidth,
            totalLength, totalHeight,
            width, heightPerLine,
            &childView,
            level + 1
        );
        utarray_push_back(timelineView->children, &childView);
        // printIndent(level);
        // printf("pushed child view\n");
    }
    
    
}

void layoutTimeline(struct TimelineNode *timeline) {
    printf("layoutTimeline\n");

    int length = getTotalTimelineLength(timeline);
    int height = getTotalTimelineHeight(timeline);
    
    freeTimelineView(&timelineView);
    GLfloat heightPerLine = (GLfloat)((GLfloat)WINDOW_HEIGHT / 2) / height;
    
    printf(
        "layoutTimeline(totalLength=%d, totalHeight=%d, width=%d, heightPerLine=%f)\n", 
        length, height, 4, heightPerLine
    );
    
    doLayoutTimeline(
        timeline, 0, (GLfloat)WINDOW_HEIGHT / 2, 
        length, height,
        WINDOW_WIDTH, 
        heightPerLine,
        &timelineView,
        0
    );
    
    printTimelineView(&timelineView, 0);
}

void addToTimeline(struct Board *board) {
    int timelineLength = utarray_len(currTimeline->snapshots);
    if (timelineLength == 0 || (currentTimestamp == timelineLength - 1)) {
        printf("Pushing to end of currTimeline\n");
        utarray_push_back(currTimeline->snapshots, board);
        currentTimestamp = utarray_len(currTimeline->snapshots) - 1;
    } else {
        printf("Forking timeline. currentTimestamp = %d, timelineLength = %d\n", currentTimestamp, timelineLength);
        // Create an alternate timeline
        int timelineLength = utarray_len(currTimeline->snapshots);
        struct TimelineNode *childTimeline1 = newTimeline(currTimeline);
        for (int i = currentTimestamp + 1; i < timelineLength; i++) {
            utarray_push_back(childTimeline1->snapshots, utarray_eltptr(currTimeline->snapshots, i));
        }
        utarray_resize(currTimeline->snapshots, currentTimestamp + 1);
        utarray_push_back(currTimeline->children, childTimeline1);
        
        struct TimelineNode *childTimeline2 = newTimeline(currTimeline);
        utarray_push_back(childTimeline2->snapshots, board);
        utarray_push_back(currTimeline->children, childTimeline2);
        currTimeline = childTimeline2;
        currentTimestamp = 0;
    }
    printf("Timeline======\n");
    printTimeline(rootTimeline, 0);
    printf("--------------\n");
    
    layoutTimeline(rootTimeline);
}

void doRenderTimeline(struct TimelineViewNode *timelineView, int level) {
    // printIndent(level);
    // printf("doRenderTimeline\n");
    struct TimelineNode *timeline = timelineView->timeline;
    int length = utarray_len(timeline->snapshots);
    GLfloat widthPerThumbnail = timelineView->height;
    GLfloat thumbnailWidth = 0.96 * widthPerThumbnail;
    GLfloat thumbnailGap = 0.04 * widthPerThumbnail;
    int numThumbnails = ceil(timelineView->width / (thumbnailWidth + thumbnailGap));
    // printIndent(level);
    // printf("doRenderTimeline(numThumbnails=%d)\n", numThumbnails);
    for (int i = 0; i < numThumbnails; i++) {
        int index = floor((float)i * length / numThumbnails);
        struct Board *board = utarray_eltptr(timeline->snapshots, index);
        struct BoardView boardView;
        boardView.x = timelineView->x + (thumbnailWidth + thumbnailGap) * i + 0.01;
        boardView.y = timelineView->y;
        boardView.size = thumbnailWidth;
        // printIndent(level);
        // printf("boardView(x=%f, y=%f, size=%f)\n", boardView.x, boardView.y, boardView.size);
        updateBoardBuffer(&glSettings, &boardView);
        updatePiecesBuffer(&glSettings, board);
        renderBoard(&glSettings, &boardView, -1, 0, 0);
    }
    // printIndent(level);
    // printf("doRenderTimeline 3\n");
    
    if (timelineView->children != NULL) {
        int numChildren = utarray_len(timelineView->children);
        for (int i = 0; i < numChildren; i++) {
            struct TimelineViewNode *childView = utarray_eltptr(timelineView->children, i);
            doRenderTimeline(childView, level + 1);
        }
    }
    // printIndent(level);
    // printf("doRenderTimeline 4\n");
}

void renderTimeline() {
    int length = utarray_len(rootTimeline->snapshots);
    if (length <= 1) {
        return;
    }
    
    doRenderTimeline(&timelineView, 0);
    
    // 
    // int numThumbnails = 4.0 / (TIMELINE_THUMBNAIL_WIDTH + TIMELINE_THUMBNAIL_GAP);
    // for (int i = 0; i < numThumbnails; i++) {
    //     int index = floor((float)i * length / numThumbnails);
    //     struct Board *board = utarray_eltptr(rootTimeline->snapshots, index);
    //     struct BoardView boardView;
    //     boardView.x = (TIMELINE_THUMBNAIL_WIDTH + TIMELINE_THUMBNAIL_GAP) * i - 2 + 0.01;
    //     boardView.y = -2;
    //     boardView.size = TIMELINE_THUMBNAIL_WIDTH;
    //     updateBoardBuffer(&glSettings, &boardView);
    //     updatePiecesBuffer(&glSettings, board);
    //     renderBoard(&glSettings, &boardView, -1, 0, 0);
    // }
}

void renderTimeMarker() {
    int timelineLength = utarray_len(rootTimeline->snapshots);
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
    memcpy(&mainBoard, utarray_eltptr(currTimeline->snapshots, currentTimestamp), sizeof(struct Board));
}

void animateTimeMarkerToTimestamp(int timestamp) {
    if (timeMarkerAnimation.targetTimestamp == timestamp) {
        return;
    }
    GLfloat srcX;
    int timelineLength = utarray_len(currTimeline->snapshots);
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
    int timelineLength = utarray_len(rootTimeline->snapshots);
    GLfloat timestampPercent = posx / WINDOW_WIDTH;
    int newCurrentTimestamp = round((timelineLength - 1) * timestampPercent);
    if (newCurrentTimestamp != currentTimestamp) {
        // animateTimeMarkerToTimestamp(newCurrentTimestamp);
        currentTimestamp = newCurrentTimestamp;
        updateMainBoard();
    }
}

void updateTimeMarkerState() {
    if (timeMarkerAnimation.endTick > 0) {
        // Update animation
        if (timeMarkerAnimation.currentTick >= timeMarkerAnimation.endTick) {
            // Animation complete
            timeMarkerAnimation.endTick = 0;
            currentTimestamp = timeMarkerAnimation.targetTimestamp;
            updateMainBoard();
        } else {
            timeMarkerAnimation.currentTick++;
        }
    }
}

void initTimeline() {
    rootTimeline = newTimeline(NULL);
    currTimeline = rootTimeline;
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
    // TODO
    draggingPieceX = posx - mainBoardView.x - (mainBoardView.size / 16.0);
    draggingPieceY = posy - mainBoardView.y - (mainBoardView.size / 16.0);
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
        int column = (int)(8.0 * (posx - mainBoardView.x) / mainBoardView.size);
        int row = (int)(8.0 * (posy - mainBoardView.y) / mainBoardView.size);
        int boardPos = row * 8 + column;
        if (action == GLFW_PRESS) {
            printf("Start drag from row = %d, column = %d, boardPos = %d\n", row, column, boardPos);
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
    if (posx >= mainBoardView.x && 
        posx <= (mainBoardView.x + mainBoardView.size) && 
        posy >= mainBoardView.y &&
        posy <= (mainBoardView.y + mainBoardView.size)) {
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
            if (currTimeline->parent != NULL) {
                currTimeline = currTimeline->parent;
                targetTimestamp = utarray_len(currTimeline->snapshots) - 1;
            } else {
                return;
            }
        }
        // animateTimeMarkerToTimestamp(targetTimestamp);
        currentTimestamp = targetTimestamp;
        updateMainBoard();
        printf("Set currentTimestamp to %d\n", currentTimestamp);
    } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        int targetTimestamp = currentTimestamp + 1;
        if (targetTimestamp >= utarray_len(currTimeline->snapshots)) {
            if (utarray_len(currTimeline->children) > 0) {
                printf("move to child timeline\n");
                currTimeline = utarray_eltptr(currTimeline->children, 0);
                targetTimestamp = 0;
            } else {
                printf("cancel\n");
                return;
            }
        }
        // animateTimeMarkerToTimestamp(targetTimestamp);
        currentTimestamp = targetTimestamp;
        updateMainBoard();
        printf("Set currentTimestamp to %d\n", currentTimestamp);
    } else if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
        if (currTimeline->parent != NULL) {
            int childIdx = utarray_eltidx(currTimeline->parent->children, currTimeline);
            int nextChildIdx = childIdx + 1;
            if (nextChildIdx >= utarray_len(currTimeline->parent->children)) {
                nextChildIdx = 0;
            }
            currTimeline = utarray_eltptr(currTimeline->parent->children, nextChildIdx);
            currentTimestamp = 0;
            updateMainBoard();
        }
        
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
    
    mainBoardView.x = (float)WINDOW_WIDTH / 4;
    mainBoardView.y = 0;
    mainBoardView.size = (float)WINDOW_WIDTH / 2;
    
    initTimeline();
    timelineView.children = NULL;
    
    addToTimeline(&mainBoard);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // glfwWaitEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        updateBoardBuffer(&glSettings, &mainBoardView);
        updatePiecesBuffer(&glSettings, &mainBoard);
        // printf("boardView.x = %f, boardView.y = %f\n", mainBoardView.x, mainBoardView.y);
        
        renderBoard(&glSettings, &mainBoardView, draggingSquare, draggingPieceX, draggingPieceY);
        renderTimeline();
        // updateTimeMarkerState();
        // renderTimeMarker();
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