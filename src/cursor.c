#include "error.h"
#include "cursor.h"
#include "trash-lang/src/stringview.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool lexLe(size_t y0, size_t x0, size_t y1, size_t x1) {
    // (y0,x0) <=_lex (y1,x1)
    return (y0 < y1) || (y0 == y1 && x0 <= x1);
}

bool isBetween(size_t ln, size_t col, CursorPos p, CursorPos q) {
    // (y0,x0) <= (ln,col) <= (y1,x1)
    return lexLe(p.ln, p.col, ln, col) && lexLe(ln, col, q.ln, q.col);
}

bool isSelecting(Cursor const* cursor) {
    return cursor->mouseSelecting || cursor->shiftSelecting;
}

bool hasSelection(Cursor const* cursor) {
    return cursor->selBegin.col != cursor->selEnd.col ||
            cursor->selBegin.ln != cursor->selEnd.ln;
}

static bool isWhitespace(char c) { return c <= ' ' || c > '~'; }
static bool isText(char c) { return isalnum(c); }

CursorPos TextBuffNextBlockPos(TextBuffer const* tb, CursorPos cur) {
    // assumes cur is a valid pos
    // wrap if at end of line
    if (cur.col == tb->lines[cur.ln]->numCols) {
        // return if past end of buff
        if (cur.ln+1 >= tb->numLines) {
            return (CursorPos) { .ln=tb->numLines-1, .col=tb->lines[tb->numLines-1]->numCols };
        }
        cur.ln += 1;
        cur.col = 0;
    }
    
    // this block logic is way simpler than others, mostly because handling all that is pain
    bool seenText = false;
    for (size_t n = tb->lines[cur.ln]->numCols;
        cur.col < n;
        ++cur.col)
    {
        char c = tb->lines[cur.ln]->buff[cur.col];
        if (isText(c)) {
            seenText = true;
        }
        else if (seenText) {
            break;
        }
    }
    
    return cur;
}

CursorPos TextBuffPrevBlockPos(TextBuffer const* tb, CursorPos cur) {
    // assumes cur is a valid pos
    // wrap if at end of line
    if (cur.col == 0) {
        // return if underflow
        if (cur.ln == 0) {
            return (CursorPos) { .ln=0, .col=0 };
        }
        cur.ln -= 1;
        cur.col = tb->lines[cur.ln]->numCols;
    }
    
    bool seenText = false;
    --cur.col;
    for (size_t n = tb->lines[cur.ln]->numCols;
        cur.col < n;
        --cur.col)
    {
        char c = tb->lines[cur.ln]->buff[cur.col];
        if (isText(c)) {
            seenText = true;
        }
        else if (seenText) {
            break;
        }
    }
    ++cur.col;
    return cur;
}



void UpdateSelection(Cursor* cursor) {
    // set selBegin and selEnd, determined by curPos and curSel
    if (lexLe(cursor->curPos.ln, cursor->curPos.col, cursor->curSel.ln, cursor->curSel.col)) {
        cursor->selBegin.col = cursor->curPos.col;
        cursor->selBegin.ln = cursor->curPos.ln;
        cursor->selEnd.col = cursor->curSel.col;
        cursor->selEnd.ln = cursor->curSel.ln;
    }
    else {
        cursor->selBegin.col = cursor->curSel.col;
        cursor->selBegin.ln = cursor->curSel.ln;
        cursor->selEnd.col = cursor->curPos.col;
        cursor->selEnd.ln = cursor->curPos.ln;
    }
}

void StopSelecting(Cursor* cursor) {
    cursor->mouseSelecting = false;
    cursor->shiftSelecting = false;
    cursor->curSel.col = cursor->curPos.col;
    cursor->curSel.ln = cursor->curPos.ln;
    cursor->selBegin.col = cursor->curPos.col;
    cursor->selBegin.ln = cursor->curPos.ln;
    cursor->selEnd.col = cursor->curPos.col;
    cursor->selEnd.ln = cursor->curPos.ln;
}

void EraseBetween(TextBuffer* tb, Cursor* cursor, CursorPos begin, CursorPos end) {
    if (begin.ln == end.ln) {
        LineBufferErase(tb->lines+begin.ln, end.col - begin.col, begin.col);
    }
    else {
        assert(end.ln > begin.ln);
        LineBufferErase(tb->lines+begin.ln,
            tb->lines[begin.ln]->numCols - begin.col,
            begin.col);
        LineBufferInsertStr(tb->lines+begin.ln,
            tb->lines[end.ln]->buff + end.col,
            tb->lines[end.ln]->numCols - end.col,
            tb->lines[begin.ln]->numCols);
        TextBufferErase(tb, end.ln-begin.ln, begin.ln+1);
    }

    cursor->curPos.ln = begin.ln;
    size_t cols = tb->lines[cursor->curPos.ln]->numCols;
    cursor->curPos.col = begin.col;
    if (cursor->curPos.col > cols)
        cursor->curPos.col = cols;
    StopSelecting(cursor);
}

void EraseSelection(TextBuffer* tb, Cursor* cursor) {
    EraseBetween(tb, cursor, cursor->selBegin, cursor->selEnd);
}

void ExtractText(TextBuffer* tb, CursorPos selBegin, CursorPos selEnd, char** outBuff, size_t* outSize) {
    size_t n = selEnd.col;
    for (size_t ln = selBegin.ln;
        ln < selEnd.ln;
        ++ln)
    {
        n += tb->lines[ln]->numCols+1;
    }
    n -= selBegin.col;
    char* buff = malloc(n+1);
    if (buff == NULL) {
        PANIC_HERE("MALLOC", "Could not allocate text buffer");
    }
    if (selBegin.ln == selEnd.ln) {
        memcpy(buff,
            tb->lines[selBegin.ln]->buff + selBegin.col,
            selEnd.col - selBegin.col);
    }
    else {
        size_t i = tb->lines[selBegin.ln]->numCols - selBegin.col;
        memcpy(buff, tb->lines[selBegin.ln]->buff + selBegin.col, i);
        buff[i++] = '\n';
        for (size_t ln = selBegin.ln+1;
            ln < selEnd.ln; ++ln)
        {
            size_t sz = tb->lines[ln]->numCols;
            memcpy(buff+i, tb->lines[ln]->buff, sz);
            i += sz;
            buff[i++] = '\n';
        }
        memcpy(buff+i, tb->lines[selEnd.ln]->buff, selEnd.col);
    }
    buff[n] = 0;
    *outBuff = buff;
    if (outSize != NULL) {
        *outSize = n;
    }
}

void InsertText(TextBuffer* tb, Cursor* cursor, const char* s, size_t n) {
    // assumes s is 'clean'
    size_t maxLines = 1024;
    size_t* lineIdx = malloc(maxLines*sizeof(size_t));
    size_t nLines = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == '\n') {
            // FIXME: lazy dynamic array, no error checking
            if (nLines > maxLines/2)
                lineIdx = realloc(lineIdx, (maxLines <<= 1)*sizeof(size_t));
            lineIdx[nLines++] = i;
        }
    }
    lineIdx[nLines] = n;

    TextBufferInsert(tb, nLines, cursor->curPos.ln+1);
    if (nLines > 0) {
        // split line
        LineBufferInsertStr(tb->lines + cursor->curPos.ln + nLines,
            tb->lines[cursor->curPos.ln]->buff + cursor->curPos.col,
            tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
            0);
        LineBufferErase(tb->lines + cursor->curPos.ln,
            tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
            cursor->curPos.col);
    }
    LineBufferInsertStr(tb->lines+(cursor->curPos.ln),
        s, lineIdx[0], cursor->curPos.col);
    cursor->curPos.col += lineIdx[0];
    for (size_t i = 0; i < nLines; ++i) {
        size_t sz = lineIdx[i+1]-lineIdx[i]-1;
        LineBufferInsertStr(tb->lines+(++cursor->curPos.ln),
            s+lineIdx[i]+1, sz, 0);
        cursor->curPos.col = sz;
    }
    free(lineIdx);
}
