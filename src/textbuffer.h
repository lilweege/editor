#ifndef TEXTBUFFER_H_
#define TEXTBUFFER_H_

#include <stdbool.h>
#include <stddef.h>

#define LB_MIN 8
#define LB_GROW_THRESH 0.75
#define LB_SHRINK_THRESH 0.25
#define TB_MIN 64
#define TB_GROW_THRESH 0.75
#define TB_SHRINK_THRESH 0.25

typedef struct {
    size_t numCols, maxCols;
    char buff[];
} LineBuffer; // alloc

typedef struct {
    size_t numLines, maxLines;
    LineBuffer** lines; // alloc
} TextBuffer;


LineBuffer* LineBufferNew(size_t initCols);
void LineBufferFree(LineBuffer* lb);
void LineBufferInsert(LineBuffer** lbP, const char* s, size_t n, size_t idx);
void LineBufferErase(LineBuffer** lbP, size_t n, size_t idx);

TextBuffer TextBufferNew(size_t initLines);
void TextBufferFree(TextBuffer tb);
void TextBufferInsert(TextBuffer* tbP, size_t idx);
void TextBufferErase(TextBuffer* tbP, size_t idx);

#endif // TEXTBUFFER_H_
