#ifndef CURSOR_H_
#define CURSOR_H_

#include "textbuffer.h"

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    size_t ln, col;
} CursorPos;

typedef struct {
    CursorPos curPos, curSel;
    CursorPos selBegin, selEnd;
    size_t colMax;
    bool shiftSelecting, mouseSelecting;
} Cursor;




bool isBetween(size_t ln, size_t col, CursorPos p, CursorPos q);
bool isSelecting(Cursor const* cursor);
bool hasSelection(Cursor const* cursor);
CursorPos TextBuffNextBlockPos(TextBuffer const* tb, CursorPos cur);
CursorPos TextBuffPrevBlockPos(TextBuffer const* tb, CursorPos cur);
void UpdateSelection(Cursor* cursor);
void StopSelecting(Cursor* cursor);
void EraseBetween(TextBuffer* tb, Cursor* cursor, CursorPos begin, CursorPos end);
void EraseSelection(TextBuffer* tb, Cursor* cursor);


#endif // CURSOR_H_
