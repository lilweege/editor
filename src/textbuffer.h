#ifndef TEXTBUFFER_H_
#define TEXTBUFFER_H_

// ideas for a better structure:
// https://en.wikipedia.org/wiki/Rope_%28data_structure%29
// https://en.wikipedia.org/wiki/Gap_buffer
// https://en.wikipedia.org/wiki/Piece_table
// https://code.visualstudio.com/blogs/2018/03/23/text-buffer-reimplementation
// https://github.com/microsoft/vscode/tree/main/src/vs/editor/common/model/pieceTreeTextBuffer

#include <stdbool.h>
#include <stddef.h>

#define LB_MIN 8
#define LB_GROW_THRESH 0.75
#define LB_SHRINK_THRESH 0.25
#ifdef NDEBUG
  #define TB_MIN 64
#else
  #define TB_MIN 8
#endif
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
void LineBufferInsertChr(LineBuffer** lbP, char c, size_t n, size_t idx);
void LineBufferInsertStr(LineBuffer** lbP, const char* s, size_t n, size_t idx);
void LineBufferErase(LineBuffer** lbP, size_t n, size_t idx);

TextBuffer TextBufferNew(size_t initLines);
void TextBufferFree(TextBuffer tb);
void TextBufferInsert(TextBuffer* tbP, size_t n, size_t idx);
void TextBufferErase(TextBuffer* tbP, size_t n, size_t idx);

#endif // TEXTBUFFER_H_
