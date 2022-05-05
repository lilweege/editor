#include "textbuffer.h"

#include "error.h"
#include <stdlib.h>
#include <string.h>

LineBuffer* LineBufferNew(size_t initCols) {
    if (initCols < LB_MIN)
        initCols = LB_MIN;
    LineBuffer* lb = malloc(sizeof(LineBuffer) + initCols);
    if (lb == NULL)
        return NULL;
    lb->numCols = 0;
    lb->maxCols = initCols;
    return lb;
}

void LineBufferFree(LineBuffer* lb) {
    free(lb);
}

static bool LineBufferRealloc(LineBuffer** lbP, size_t newMax) {
    if (newMax < LB_MIN)
        newMax = LB_MIN;
    LineBuffer* lb = realloc(*lbP, sizeof(LineBuffer) + newMax);
    if (lb == NULL)
        return false;
    (*lbP = lb)->maxCols = newMax;
    return true;
}

void LineBufferInsert(LineBuffer** lbP, const char* s, size_t n, size_t idx) {
    // grow
    size_t newMax = (*lbP)->maxCols;
    while ((*lbP)->numCols + n > newMax * LB_GROW_THRESH)
        newMax <<= 1;
    if (newMax > (*lbP)->maxCols && !LineBufferRealloc(lbP, newMax))
        PANIC_HERE("MALLOC", "Could not grow LineBuffer");
    // make space and insert
    memmove((*lbP)->buff+idx+n, (*lbP)->buff+idx, (*lbP)->numCols-idx);
    memcpy((*lbP)->buff+idx, s, n);
    (*lbP)->numCols += n;
}

void LineBufferErase(LineBuffer** lbP, size_t n, size_t idx) {
    // erase
    (*lbP)->numCols -= n;
    memmove((*lbP)->buff+idx, (*lbP)->buff+idx+n, (*lbP)->numCols-idx);
    // shrink
    size_t newMax = (*lbP)->maxCols;
    while ((*lbP)->numCols < newMax * LB_SHRINK_THRESH && newMax > LB_MIN)
        newMax >>= 1;
    if (newMax < (*lbP)->maxCols && !LineBufferRealloc(lbP, newMax))
        PANIC_HERE("MALLOC", "Could not shrink LineBuffer"); // probably unreachable
}


TextBuffer TextBufferNew(size_t initLines) {
    TextBuffer tb = {
        .lines = malloc(initLines * sizeof(LineBuffer*)),
        .numLines = 1,
        .maxLines = initLines,
    };
    if (tb.lines == NULL)
        PANIC_HERE("MALLOC", "Could not allocate TextBuffer");
    for (size_t i = 0; i < tb.maxLines; ++i)
        tb.lines[i] = LineBufferNew(LB_MIN);
    return tb;
}

void TextBufferFree(TextBuffer tb) {
    for (size_t i = 0; i < tb.maxLines; ++i)
        LineBufferFree(tb.lines[i]);
    free(tb.lines);
}

static bool TextBufferRealloc(TextBuffer* tbP, size_t newMax) {
    printf("REALLOC TB\n");
    if (newMax < LB_MIN)
        newMax = LB_MIN;
    LineBuffer** newLines = realloc(tbP->lines, newMax * sizeof(LineBuffer*));
    if (newLines == NULL)
        return false;
    tbP->lines = newLines;
    tbP->maxLines = newMax;
    return true;
}

// TODO: check correctness
// my brain is turned off right now
void TextBufferInsert(TextBuffer* tbP, size_t idx) {
    if (++tbP->numLines > tbP->maxLines * TB_GROW_THRESH && !TextBufferRealloc(tbP, tbP->numLines << 1))
        PANIC_HERE("MALLOC", "Could not grow TextBuffer");
    memmove(tbP->lines+idx+1, tbP->lines+idx, tbP->numLines-idx);
    tbP->lines[idx] = LineBufferNew(LB_MIN);
}

void TextBufferErase(TextBuffer* tbP, size_t idx) {
    LineBufferFree(tbP->lines[idx]);
    memmove(tbP->lines+idx, tbP->lines+idx+1, tbP->numLines-idx);
    if (--tbP->numLines > tbP->maxLines * TB_SHRINK_THRESH && !TextBufferRealloc(tbP, tbP->numLines >> 1))
        PANIC_HERE("MALLOC", "Could not shrink TextBuffer");
}



