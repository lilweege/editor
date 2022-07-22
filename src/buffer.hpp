#ifndef BUFFER_H_
#define BUFFER_H_

#include <stddef.h>
#include <stdbool.h>
#include <vector>
#include <immer/vector.hpp>

struct CursorPos {
    size_t ln, col;
};

struct Cursor {
    CursorPos curPos, curSel;
    CursorPos selBegin, selEnd;
    size_t colMax;
    bool shiftSelecting, mouseSelecting;
};

typedef std::vector<char> Line;
typedef std::vector<Line> Text;

struct Buffer {
    Text text;
    Cursor cursor;
};


bool isBetween(size_t ln, size_t col, CursorPos p, CursorPos q);
bool isSelecting(Cursor const& cursor);
bool hasSelection(Cursor const& cursor);
CursorPos TextBuffNextBlockPos(Text const& text, CursorPos cur);
CursorPos TextBuffPrevBlockPos(Text const& text, CursorPos cur);
void UpdateSelection(Cursor& cursor);
void StopSelecting(Cursor& cursor);
void EraseBetween(Text& text, CursorPos begin, CursorPos end);
void EraseSelection(Text& text, Cursor const& cursor);
void ResetCursor(Text& text, Cursor& cursor, CursorPos begin);
void ExtractText(Text& text, CursorPos selBegin, CursorPos selEnd, char** outBuff, size_t* outSize);
void InsertCStr(Text& text, CursorPos& curPos, const char* s, size_t n);
void InsertText(Text& text, CursorPos& curPos, Line const& line, size_t begin, size_t end);



#endif // BUFFER_H_

