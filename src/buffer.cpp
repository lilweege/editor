#include "error.hpp"
#include "buffer.hpp"
#include "trash-lang/src/stringview.h"

#include <vector>

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

bool isSelecting(Cursor const& cursor) {
    return cursor.mouseSelecting || cursor.shiftSelecting;
}

bool hasSelection(Cursor const& cursor) {
    return cursor.selBegin.col != cursor.selEnd.col ||
            cursor.selBegin.ln != cursor.selEnd.ln;
}

// static bool isWhitespace(char c) { return c <= ' ' || c > '~'; }
static bool isText(char c) { return isalnum(c); }

CursorPos TextBuffNextBlockPos(Text const& text, CursorPos cur) {
    // assumes cur is a valid pos
    // wrap if at end of line
    if (cur.col == text[cur.ln].size()) {
        // return if past end of buff
        if (cur.ln+1 >= text.size()) {
            return (CursorPos) { .ln=text.size()-1, .col=text[text.size()-1].size() };
        }
        cur.ln += 1;
        cur.col = 0;
    }

    // this block logic is way simpler than others, mostly because handling all that is pain
    bool seenText = false;
    for (size_t n = text[cur.ln].size();
        cur.col < n;
        ++cur.col)
    {
        char c = text[cur.ln][cur.col];
        if (isText(c)) {
            seenText = true;
        }
        else if (seenText) {
            break;
        }
    }

    return cur;
}

CursorPos TextBuffPrevBlockPos(Text const& text, CursorPos cur) {
    // assumes cur is a valid pos
    // wrap if at end of line
    if (cur.col == 0) {
        // return if underflow
        if (cur.ln == 0) {
            return (CursorPos) { .ln=0, .col=0 };
        }
        cur.ln -= 1;
        cur.col = text[cur.ln].size();
    }

    bool seenText = false;
    --cur.col;
    for (size_t n = text[cur.ln].size();
        cur.col < n;
        --cur.col)
    {
        char c = text[cur.ln][cur.col];
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



void UpdateSelection(Cursor& cursor) {
    // set selBegin and selEnd, determined by curPos and curSel
    if (lexLe(cursor.curPos.ln, cursor.curPos.col, cursor.curSel.ln, cursor.curSel.col)) {
        cursor.selBegin.col = cursor.curPos.col;
        cursor.selBegin.ln = cursor.curPos.ln;
        cursor.selEnd.col = cursor.curSel.col;
        cursor.selEnd.ln = cursor.curSel.ln;
    }
    else {
        cursor.selBegin.col = cursor.curSel.col;
        cursor.selBegin.ln = cursor.curSel.ln;
        cursor.selEnd.col = cursor.curPos.col;
        cursor.selEnd.ln = cursor.curPos.ln;
    }
}

void StopSelecting(Cursor& cursor) {
    cursor.mouseSelecting = false;
    cursor.shiftSelecting = false;
    cursor.curSel.col = cursor.curPos.col;
    cursor.curSel.ln = cursor.curPos.ln;
    cursor.selBegin.col = cursor.curPos.col;
    cursor.selBegin.ln = cursor.curPos.ln;
    cursor.selEnd.col = cursor.curPos.col;
    cursor.selEnd.ln = cursor.curPos.ln;
}

void EraseBetween(Text& text, CursorPos begin, CursorPos end) {
    if (begin.ln == end.ln) {
        text[begin.ln].erase(text[begin.ln].begin()+begin.col, text[begin.ln].begin()+end.col);
    }
    else {
        assert(end.ln > begin.ln);
        text[begin.ln].erase(text[begin.ln].begin()+begin.col, text[begin.ln].end());
        text[begin.ln].insert(text[begin.ln].end(), text[end.ln].begin()+end.col, text[end.ln].end());
        text.erase(text.begin()+begin.ln+1, text.begin()+end.ln+1);
    }
}

void EraseSelection(Text& text, Cursor const& cursor) {
    EraseBetween(text, cursor.selBegin, cursor.selEnd);
}

void ResetCursor(Text& text, Cursor& cursor, CursorPos begin) {
    cursor.curPos.ln = begin.ln;
    size_t cols = text[cursor.curPos.ln].size();
    cursor.curPos.col = begin.col;
    if (cursor.curPos.col > cols)
        cursor.curPos.col = cols;
}

void ExtractText(Text& text, CursorPos selBegin, CursorPos selEnd, char** outBuff, size_t* outSize) {
    size_t n = selEnd.col;
    for (size_t ln = selBegin.ln;
        ln < selEnd.ln;
        ++ln)
    {
        n += text[ln].size()+1;
    }
    n -= selBegin.col;
    char* buff = (char*) malloc(n+1);
    if (buff == NULL) {
        PANIC_HERE("MALLOC", "Could not allocate text buffer");
    }
    if (selBegin.ln == selEnd.ln) {
        if (text[selBegin.ln].data() != NULL) {
            memcpy(buff,
                text[selBegin.ln].data() + selBegin.col,
                selEnd.col - selBegin.col);
        }
    }
    else {
        size_t i = text[selBegin.ln].size() - selBegin.col;
        if (text[selBegin.ln].data() != NULL) {
            memcpy(buff, text[selBegin.ln].data() + selBegin.col, i);
        }
        buff[i++] = '\n';
        for (size_t ln = selBegin.ln+1;
            ln < selEnd.ln; ++ln)
        {
            size_t sz = text[ln].size();
            if (text[ln].data() != NULL) {
                memcpy(buff+i, text[ln].data(), sz);
            }
            i += sz;
            buff[i++] = '\n';
        }
        if (text[selEnd.ln].data() != NULL) {
            memcpy(buff+i, text[selEnd.ln].data(), selEnd.col);
        }
    }
    buff[n] = 0;
    *outBuff = buff;
    if (outSize != NULL) {
        *outSize = n;
    }
}

void InsertCStr(Text& text, CursorPos& curPos, const char* s, size_t n) {
    // assumes s is 'clean'
    std::vector<size_t> lineIdx;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == '\n') {
            // FIXME: lazy dynamic array, no error checking
            lineIdx.push_back(i);
        }
    }
    size_t nLines = lineIdx.size();
    lineIdx.push_back(n);

    if (nLines > 0) {
        // split line
        text.insert(text.begin()+curPos.ln+1, nLines, Line{});
        text[curPos.ln+nLines].insert(text[curPos.ln+nLines].begin(),
            text[curPos.ln].begin()+curPos.col,
            text[curPos.ln].end());
        text[curPos.ln].erase(
            text[curPos.ln].begin()+curPos.col,
            text[curPos.ln].end());
    }
    text[curPos.ln].insert(
        text[curPos.ln].begin()+curPos.col,
        s, s+lineIdx[0]);
    curPos.col += lineIdx[0];
    for (size_t i = 0; i < nLines; ++i) {
        size_t sz = lineIdx[i+1]-lineIdx[i]-1;
        ++curPos.ln;
        text[curPos.ln].insert(
            text[curPos.ln].begin(),
            s+lineIdx[i]+1, s+lineIdx[i]+1+sz);
        curPos.col = sz;
    }
}

void InsertText(Text& text, CursorPos& curPos, Line const& line, size_t begin, size_t end) {
    InsertCStr(text, curPos, line.data()+begin, end-begin);
}

