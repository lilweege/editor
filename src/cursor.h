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
bool isSelecting(Cursor const& cursor);
bool hasSelection(Cursor const& cursor);
CursorPos TextBuffNextBlockPos(Text const& text, CursorPos cur);
CursorPos TextBuffPrevBlockPos(Text const& text, CursorPos cur);
void UpdateSelection(Cursor& cursor);
void StopSelecting(Cursor& cursor);
void EraseBetween(Text& text, Cursor& cursor, CursorPos begin, CursorPos end);
void EraseSelection(Text& text, Cursor& cursor);

void ExtractText(Text& text, CursorPos selBegin, CursorPos selEnd, char** outBuff, size_t* outSize);
void InsertText(Text& text, Cursor& cursor, const char* s, size_t n);

#endif // CURSOR_H_
