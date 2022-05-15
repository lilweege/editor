#include "textbuffer.h"
#include "error.h"
#include "config.h"
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "whereami.h"
#include <SDL2/SDL.h>
#include <stdlib.h>


typedef struct {
    size_t ln, col;
} CursorPos;

typedef struct {
    CursorPos curPos, curSel;
    CursorPos selBegin, selEnd;
    size_t colMax;
    bool shiftSelecting, mouseSelecting;
} Cursor;

static bool isWhitespace(char c) {
    return c <= ' ' || c > '~';
}

static CursorPos TextBuffNextBlockPos(TextBuffer const* tb, CursorPos cur) {
    // assumes cur is a valid pos
    bool seenDelim = false;
    // wrap if at end of line
    if (cur.col == tb->lines[cur.ln]->numCols) {
        // return if past end of buff
        if (cur.ln+1 >= tb->numLines) {
            return (CursorPos) { .ln=tb->numLines-1, .col=tb->lines[tb->numLines-1]->numCols };
        }
        cur.ln += 1;
        cur.col = 0;
        seenDelim = true;
    }
    
    // this block logic is way simpler than others, mostly because handling all that is pain
    
    for (size_t n = tb->lines[cur.ln]->numCols;
        cur.col < n;
        ++cur.col)
    {
        char c = tb->lines[cur.ln]->buff[cur.col];
        if (isWhitespace(c)) {
            seenDelim = true;
        }
        else if (seenDelim) {
            break;
        }
    }
    
    return cur;
}

static CursorPos TextBuffPrevBlockPos(TextBuffer const* tb, CursorPos cur) {
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
        if (isWhitespace(c)) {
            if (seenText)
                break;
        }
        else {
            seenText = true;
        }
    }
    ++cur.col;
    return cur;
}

static bool lexLe(size_t y0, size_t x0, size_t y1, size_t x1) {
    // (y0,x0) <=_lex (y1,x1)
    return (y0 < y1) || (y0 == y1 && x0 <= x1);
}

static bool isBetween(size_t ln, size_t col, size_t y0, size_t x0, size_t y1, size_t x1) {
    // (y0,x0) <= (ln,col) <= (y1,x1)
    return lexLe(y0, x0, ln, col) && lexLe(ln, col, y1, x1);
}


static bool isSelecting(Cursor const* cursor) {
    return cursor->mouseSelecting || cursor->shiftSelecting;
}

static bool hasSelection(Cursor const* cursor) {
    return cursor->selBegin.col != cursor->selEnd.col ||
            cursor->selBegin.ln != cursor->selEnd.ln;
}

static void UpdateSelection(Cursor* cursor) {
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

static void StopSelecting(Cursor* cursor) {
    cursor->mouseSelecting = false;
    cursor->shiftSelecting = false;
    cursor->curSel.col = cursor->curPos.col;
    cursor->curSel.ln = cursor->curPos.ln;
    cursor->selBegin.col = cursor->curPos.col;
    cursor->selBegin.ln = cursor->curPos.ln;
    cursor->selEnd.col = cursor->curPos.col;
    cursor->selEnd.ln = cursor->curPos.ln;
}

static void EraseBetween(TextBuffer* tb, Cursor* cursor, CursorPos begin, CursorPos end) {
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

static void EraseSelection(TextBuffer* tb, Cursor* cursor) {
    EraseBetween(tb, cursor, cursor->selBegin, cursor->selEnd);
}

static void RenderCharacter(SDL_Renderer* renderer, SDL_Texture* fontTexture, int fontCharWidth, int fontCharHeight, char ch, int x, int y, float scl) {
    size_t idx = ch - ASCII_PRINTABLE_MIN;
    SDL_Rect sourceRect = {
        .x = (int)(idx * fontCharWidth),
        .y = 0,
        .w = (int)fontCharWidth,
        .h = (int)fontCharHeight,
    };

    SDL_Rect screenRect = {
        .x = x,
        .y = y,
        .w = (int)(fontCharWidth * scl),
        .h = (int)(fontCharHeight * scl),
    };
    // TODO: switch to opengl so this can be batch rendered
    SDL_CHECK_CODE(SDL_RenderCopy(renderer, fontTexture, &sourceRect, &screenRect));
}

static void HandleTextInput(TextBuffer* tb, Cursor* cursor, SDL_TextInputEvent const* event) {
    if (hasSelection(cursor)) {
        EraseSelection(tb, cursor);
    }
    const char* s = event->text;
    size_t n = strlen(s);
    // NOTE: does not handle s with any newline
    LineBufferInsertStr(tb->lines+cursor->curPos.ln, s, n, cursor->curPos.col);
    cursor->curPos.col += n;
    cursor->colMax = cursor->curPos.col;
    StopSelecting(cursor);
}

static void HandleKeyDown(TextBuffer* tb, Cursor* cursor, SDL_KeyboardEvent const* event) {
    // TODO: cursor, text selection, clipboard, zoom, undo/redo, line move, multi-cursor
    // keys: backspace, delete, enter, home, end, pgup, pgdown, left/right, up/down, tab
    // mods: ctrl, shift, alt
    SDL_Keycode code = event->keysym.sym;
    SDL_Keymod mod = event->keysym.mod;
    bool ctrlPressed = mod & KMOD_CTRL,
        shiftPressed = mod & KMOD_SHIFT;
    // TODO: slight optimizations, when splitting the current line, choose smaller half to reinsert
    // do this for enter, backspace, delete
    if (code == SDLK_RETURN) {
        if (hasSelection(cursor)) {
            EraseSelection(tb, cursor);
        }
        TextBufferInsert(tb, 1, cursor->curPos.ln + 1);
        LineBufferInsertStr(tb->lines + cursor->curPos.ln + 1,
            tb->lines[cursor->curPos.ln]->buff + cursor->curPos.col,
            tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
            0);
        LineBufferErase(tb->lines + cursor->curPos.ln,
            tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
            cursor->curPos.col);
        cursor->curPos.col = 0;
        cursor->curPos.ln += 1;
        cursor->colMax = cursor->curPos.col;
    }
    else if (code == SDLK_TAB) {
        // TODO: shift mod -> dedent
        if (hasSelection(cursor)) {
            for (size_t line = cursor->selBegin.ln;
                line <= cursor->selEnd.ln;
                ++line)
            {
                LineBufferInsertChr(tb->lines+line, ' ', TabSize, 0);
            }
            cursor->curPos.col += TabSize;
            cursor->curSel.col += TabSize;
            UpdateSelection(cursor);
        }
        else {
            LineBufferInsertChr(tb->lines+cursor->curPos.ln, ' ', TabSize, cursor->curPos.col);
            cursor->curPos.col += TabSize;
            cursor->colMax = cursor->curPos.col;
        }
    }
    else if (code == SDLK_BACKSPACE) {
        if (hasSelection(cursor)) {
            EraseSelection(tb, cursor);
        }
        else if (ctrlPressed) {
            EraseBetween(tb, cursor,
                TextBuffPrevBlockPos(tb, cursor->curPos),
                cursor->curPos);
        }
        else {
            if (cursor->curPos.col >= 1) {
                cursor->curPos.col -= 1;
                LineBufferErase(tb->lines+cursor->curPos.ln, 1, cursor->curPos.col);
            }
            else if (cursor->curPos.ln != 0) {
                size_t oldCols = tb->lines[cursor->curPos.ln-1]->numCols;
                LineBufferInsertStr(tb->lines+cursor->curPos.ln-1,
                    tb->lines[cursor->curPos.ln]->buff + cursor->curPos.col,
                    tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
                    tb->lines[cursor->curPos.ln-1]->numCols);
                cursor->curPos.col = oldCols;
                TextBufferErase(tb, 1, cursor->curPos.ln);
                cursor->curPos.ln -= 1;
            }
        }
        cursor->colMax = cursor->curPos.col;
    }
    else if (code == SDLK_DELETE) {
        if (hasSelection(cursor)) {
            EraseSelection(tb, cursor);
        }
        else if (ctrlPressed) {
            EraseBetween(tb, cursor,
                cursor->curPos,
                TextBuffNextBlockPos(tb, cursor->curPos));
        }
        else {
            if (cursor->curPos.col + 1 <= tb->lines[cursor->curPos.ln]->numCols) {
                LineBufferErase(tb->lines+cursor->curPos.ln, 1, cursor->curPos.col);
            }
            else if (cursor->curPos.ln != tb->numLines-1) {
                LineBufferInsertStr(tb->lines+cursor->curPos.ln,
                    tb->lines[cursor->curPos.ln+1]->buff,
                    tb->lines[cursor->curPos.ln+1]->numCols,
                    tb->lines[cursor->curPos.ln]->numCols);
                TextBufferErase(tb, 1, cursor->curPos.ln+1);
            }
        }
        cursor->colMax = cursor->curPos.col;
    }
    else if (code == SDLK_LEFT ||
            code == SDLK_RIGHT ||
            code == SDLK_UP ||
            code == SDLK_DOWN ||
            code == SDLK_HOME ||
            code == SDLK_END)
    { // directional keys
        if (hasSelection(cursor)) {
            cursor->shiftSelecting = true;
        }
        bool wasSelecting = cursor->shiftSelecting;
        if (shiftPressed) {
            if (!cursor->shiftSelecting) {
                cursor->curSel.col = cursor->curPos.col;
                cursor->curSel.ln = cursor->curPos.ln;
                UpdateSelection(cursor);
                cursor->shiftSelecting = true;
            }
        }
        else {
            StopSelecting(cursor);
        }
        wasSelecting = wasSelecting && !isSelecting(cursor);

        switch (code) {
            case SDLK_LEFT: {
                if (wasSelecting) {
                    cursor->curPos.col = cursor->selBegin.col;
                    cursor->curPos.ln = cursor->selBegin.ln;
                }
                else if (ctrlPressed) {
                    CursorPos prvPos = TextBuffPrevBlockPos(tb, cursor->curPos);
                    cursor->curPos.col = prvPos.col;
                    cursor->curPos.ln = prvPos.ln;
                }
                else {
                    if (cursor->curPos.col >= 1) {
                        cursor->curPos.col -= 1;
                    }
                    else if (cursor->curPos.ln >= 1) {
                        cursor->curPos.ln -= 1;
                        cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
                    }
                }
                cursor->colMax = cursor->curPos.col;
            } break;
            case SDLK_RIGHT: {
                if (wasSelecting) {
                    cursor->curPos.col = cursor->selEnd.col;
                    cursor->curPos.ln = cursor->selEnd.ln;
                }
                else if (ctrlPressed) {
                    CursorPos nxtPos = TextBuffNextBlockPos(tb, cursor->curPos);
                    cursor->curPos.col = nxtPos.col;
                    cursor->curPos.ln = nxtPos.ln;
                }
                else {
                    if (cursor->curPos.col + 1 <= tb->lines[cursor->curPos.ln]->numCols) {
                        cursor->curPos.col += 1;
                    }
                    else if (cursor->curPos.ln + 1 < tb->numLines) {
                        cursor->curPos.col = 0;
                        cursor->curPos.ln += 1;
                    }
                }
                cursor->colMax = cursor->curPos.col;
            } break;
            case SDLK_UP: {
                if (cursor->curPos.ln >= 1) {
                    cursor->curPos.ln -= 1;
                    if (cursor->curPos.col < cursor->colMax)
                        cursor->curPos.col = cursor->colMax;
                    size_t cols = tb->lines[cursor->curPos.ln]->numCols;
                    if (cursor->curPos.col > cols)
                        cursor->curPos.col = cols;
                }
                else {
                    cursor->curPos.ln = 0;
                    cursor->curPos.col = 0;
                    cursor->colMax = cursor->curPos.col;
                }
            } break;
            case SDLK_DOWN: {
                if (cursor->curPos.ln + 1 < tb->numLines) {
                    cursor->curPos.ln += 1;
                    if (cursor->curPos.col < cursor->colMax)
                        cursor->curPos.col = cursor->colMax;
                    size_t cols = tb->lines[cursor->curPos.ln]->numCols;
                    if (cursor->curPos.col > cols)
                        cursor->curPos.col = cols;
                }
                else {
                    cursor->curPos.ln = tb->numLines-1;
                    cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
                    cursor->colMax = cursor->curPos.col;
                }
            } break;
            case SDLK_HOME: {
                cursor->curPos.col = 0;
                cursor->colMax = cursor->curPos.col;
            } break;
            case SDLK_END: {
                cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
                cursor->colMax = cursor->curPos.col;
            } break;
            default: assert(0 && "Unreachable"); break;
        }
    }
    // TODO: ctrl+o,s,d,f,z,y
    else if (code == SDLK_a && ctrlPressed) {
        cursor->curSel.col = 0;
        cursor->curSel.ln = 0;
        cursor->curPos.ln = tb->numLines-1;
        cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
        UpdateSelection(cursor);
    }
    else if ((code == SDLK_c || code == SDLK_x) && ctrlPressed) {
        // cut/copy
        size_t n = cursor->selEnd.col;
        for (size_t ln = cursor->selBegin.ln;
            ln < cursor->selEnd.ln;
            ++ln)
        {
            n += tb->lines[ln]->numCols+1;
        }
        n -= cursor->selBegin.col;
        char* buff = malloc(n+1);
        if (buff == NULL) {
            PANIC_HERE("MALLOC", "Could not allocate clipboard buffer");
        }
        if (cursor->selBegin.ln == cursor->selEnd.ln) {
            memcpy(buff,
                tb->lines[cursor->selBegin.ln]->buff + cursor->selBegin.col,
                cursor->selEnd.col - cursor->selBegin.col);
        }
        else {
            size_t i = tb->lines[cursor->selBegin.ln]->numCols - cursor->selBegin.col;
            memcpy(buff, tb->lines[cursor->selBegin.ln]->buff + cursor->selBegin.col, i);
            buff[i++] = '\n';
            for (size_t ln = cursor->selBegin.ln+1;
                ln < cursor->selEnd.ln; ++ln)
            {
                size_t sz = tb->lines[ln]->numCols;
                memcpy(buff+i, tb->lines[ln]->buff, sz);
                i += sz;
                buff[i++] = '\n';
            }
            memcpy(buff+i, tb->lines[cursor->selEnd.ln]->buff, cursor->selEnd.col);
        }
        buff[n] = 0;
        SDL_SetClipboardText(buff);
        free(buff);
        if (code == SDLK_x) { // cut
            EraseSelection(tb, cursor);
        }
    }
    else if (code == SDLK_v && ctrlPressed) {
        // paste
        if (SDL_HasClipboardText()) {
            if (hasSelection(cursor)) {
                EraseSelection(tb, cursor);
            }
            char* clip = SDL_GetClipboardText();
            assert(clip != NULL);
            if (clip[0] == '\0') {
                SDL_ERROR_HERE();
            }
            size_t lineIdx[1024]; // FIXME
            size_t nLines = 0;
            size_t n = 0;
            for (; clip[n] != '\0'; ++n) {
                if (clip[n] == '\n') {
                    lineIdx[nLines++] = n;
                    assert(nLines < 1024);
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
                clip, lineIdx[0], cursor->curPos.col);
            cursor->curPos.col += lineIdx[0];
            for (size_t i = 0; i < nLines; ++i) {
                size_t sz = lineIdx[i+1]-lineIdx[i]-1;
                LineBufferInsertStr(tb->lines+(++cursor->curPos.ln),
                    clip+lineIdx[i]+1, sz, 0);
                cursor->curPos.col = sz;
            }
            SDL_free(clip);
        }
    }

    if (isSelecting(cursor)) {
        UpdateSelection(cursor);
    }
}

static void GetScreenSize(SDL_Window* window, int charWidth, int charHeight, size_t* sRows, size_t* sCols) {
    int sw, sh;
    SDL_GetWindowSize(window, &sw, &sh);
    *sRows = sh / charHeight;
    *sCols = sw / charWidth;
}

static void CursorAutoscroll(size_t* topLine, size_t cursorLn, size_t sRows) {
    if (cursorLn < *topLine) {
        *topLine = cursorLn;
    }
    else if (cursorLn > *topLine+(sRows-1)) {
        *topLine = cursorLn-(sRows-1);
    }
}

static void CalculateLeftMargin(size_t* leftMarginBegin, size_t* leftMarginEnd, size_t charWidth, size_t numLines) {
    // numLines should never be zero so log is fine
    *leftMarginBegin = (size_t)((LineMarginLeft + (int)log10((float)numLines) + 1) * charWidth);
    *leftMarginEnd = (size_t)(*leftMarginBegin + LineMarginRight * charWidth);
}

static void ScreenToCursor(Cursor* cursor, size_t mouseX, size_t mouseY, TextBuffer const* textBuff, size_t leftMarginEnd, size_t topLine, int charWidth, int charHeight) {
    // offset due to line numbers
    if (mouseX < leftMarginEnd) mouseX = 0;
    else mouseX -= leftMarginEnd;
    // round forward or back to nearest char
    mouseX += charWidth / 2;
    mouseY += topLine * charHeight;

    cursor->curPos.col = mouseX / charWidth;
    cursor->curPos.ln = mouseY / charHeight;

    // clamp y
    if (cursor->curPos.ln > textBuff->numLines-1)
        cursor->curPos.ln = textBuff->numLines-1;

    // clamp x
    size_t maxCols = textBuff->lines[cursor->curPos.ln]->numCols;
    if (cursor->curPos.col > maxCols)
        cursor->curPos.col = maxCols;
}


int main(int argc, char** argv) {
    (void) argc; (void) argv;
    SDL_CHECK_CODE(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* const window = SDL_CHECK_PTR(
        SDL_CreateWindow(ProgramTitle,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            InitialWindowWidth, InitialWindowHeight,
            SDL_WINDOW_RESIZABLE));
    SDL_Renderer* const renderer = SDL_CHECK_PTR(
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));



    // absolute path of asset, so fopen can locate the asset no matter the cwd
    int pathSz = wai_getExecutablePath(NULL, 0, NULL);
    char* fontPath = (char*)malloc(pathSz + 1 + strlen(FontFilename));
    int dirnameSz;
    wai_getExecutablePath(fontPath, pathSz, &dirnameSz);
    strcpy(fontPath+dirnameSz+1, FontFilename);

    // image -> surface -> texture
    int imgWidth, imgHeight, imgComps;
    unsigned char const* const imgData = STBI_CHECK_PTR(
        stbi_load(fontPath, &imgWidth, &imgHeight, &imgComps, STBI_rgb_alpha));
    free(fontPath);
    
    int const depth = 32;
    int const pitch = 4 * imgWidth;
    Uint32 const
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000,
        gmask = 0x00ff0000,
        bmask = 0x0000ff00,
        amask = 0x000000ff;
    #else // little endian, like x86
        rmask = 0x000000ff,
        gmask = 0x0000ff00,
        bmask = 0x00ff0000,
        amask = 0xff000000;
    #endif

    SDL_Surface* fontSurface = STBI_CHECK_PTR(
        SDL_CreateRGBSurfaceFrom((void*)imgData, imgWidth, imgHeight,
            depth, pitch, rmask, gmask, bmask, amask));
    SDL_Texture* fontTexture = SDL_CHECK_PTR(
        SDL_CreateTextureFromSurface(renderer, fontSurface));
    SDL_FreeSurface(fontSurface);

    SDL_Cursor* const mouseCursorArrow = SDL_CHECK_PTR(
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
    SDL_Cursor* const mouseCursorIBeam = SDL_CHECK_PTR(
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));
    SDL_Cursor const* currentMouseCursor = mouseCursorArrow;
    

    int const fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
    int const fontCharHeight = imgHeight;
    int const charWidth = (int)(fontCharWidth * FontScale);
    int const charHeight = (int)(fontCharHeight * FontScale);
    TextBuffer textBuff = TextBufferNew(8);
    Cursor cursor = {0};
    size_t topLine = 0, sRows, sCols, leftMarginBegin, leftMarginEnd;

    GetScreenSize(window, charWidth, charHeight, &sRows, &sCols);
    CalculateLeftMargin(&leftMarginBegin, &leftMarginEnd, charWidth, textBuff.numLines);

    // main loop
    Uint32 const timestep = 1000 / TargetFPS;
    for (bool quit = false; !quit;) {

        Uint32 startTick = SDL_GetTicks();
        bool needsRedraw = false;

        // handle events
        SDL_Event e;
        while (SDL_PollEvent(&e)) switch (e.type) {

            case SDL_QUIT: {
                quit = true;
            } break;

            case SDL_MOUSEMOTION: {
                // update cursor style
                if (currentMouseCursor != mouseCursorIBeam && (size_t)e.motion.x > leftMarginEnd) {
                    currentMouseCursor = mouseCursorIBeam;
                    SDL_SetCursor(mouseCursorIBeam);
                    needsRedraw = true;
                }
                else if (currentMouseCursor != mouseCursorArrow && (size_t)e.motion.x <= leftMarginEnd) {
                    currentMouseCursor = mouseCursorArrow;
                    SDL_SetCursor(mouseCursorArrow);
                    needsRedraw = true;
                }
                
                // mouse drag selection
                if (cursor.mouseSelecting) {
                    size_t mouseX = e.motion.x < 0 ? 0 : e.motion.x;
                    size_t mouseY = e.motion.y < 0 ? 0 : e.motion.y;
                    ScreenToCursor(&cursor, mouseX, mouseY, &textBuff, leftMarginEnd, topLine, charWidth, charHeight);
                    UpdateSelection(&cursor);
                    needsRedraw = true;
                }
            } break;

            case SDL_MOUSEBUTTONDOWN: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cursor.mouseSelecting = true;
                    size_t mouseX = e.button.x < 0 ? 0 : e.button.x;
                    size_t mouseY = e.button.y < 0 ? 0 : e.button.y;

                    ScreenToCursor(&cursor, mouseX, mouseY, &textBuff, leftMarginEnd, topLine, charWidth, charHeight);
                    cursor.colMax = cursor.curPos.col;
                    
                    cursor.curSel.col = cursor.curPos.col;
                    cursor.curSel.ln = cursor.curPos.ln;
                    UpdateSelection(&cursor);
                    needsRedraw = true;
                }
            } break;

            case SDL_MOUSEBUTTONUP: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cursor.mouseSelecting = false;
                    needsRedraw = true;
                }
            } break;

            case SDL_MOUSEWHEEL: {
                // TODO: horizontal scrolling
                Sint32 dy = e.wheel.y;
                if ((dy > 0 && topLine >= (size_t)dy) || // up
                    (dy < 0 && topLine + (-dy) < textBuff.numLines)) // down
                {
                    topLine -= dy;
                    needsRedraw = true;
                }
            } break;

            case SDL_WINDOWEVENT: { // https://wiki.libsdl.org/SDL_WindowEvent
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    GetScreenSize(window, charWidth, charHeight, &sRows, &sCols);
                    needsRedraw = true;
                }
            } break;

            case SDL_TEXTINPUT: {
                HandleTextInput(&textBuff, &cursor, &e.text);
                CursorAutoscroll(&topLine, cursor.curPos.ln, sRows);
                needsRedraw = true;
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(&textBuff, &cursor, &e.key);
                CursorAutoscroll(&topLine, cursor.curPos.ln, sRows);
                // on enter or delete, number of lines can change -> recalculate margin
                CalculateLeftMargin(&leftMarginBegin, &leftMarginEnd, charWidth, textBuff.numLines);
                needsRedraw = true;
            } break;
        }
        
        if (needsRedraw) {
            // draw bg
            SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteBG), 255));
            SDL_CHECK_CODE(SDL_RenderClear(renderer));

            // TODO: this loop will change when syntax highlighting is added
            SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteHL), 255)); // selection highlight
            SDL_CHECK_CODE(SDL_SetTextureColorMod(fontTexture, COL_RGB(PaletteFG))); // font
            
            for (size_t y = topLine;
                y <= topLine+sRows && y < textBuff.numLines;
                ++y)
            {
                int const sy = (int)((y-topLine) * charHeight);

                // draw line number
                int sx = (int)leftMarginBegin;
                for (size_t lineNum = y+1; lineNum > 0; lineNum /= 10) {
                    sx -= charWidth;
                    RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, lineNum % 10 + '0', sx, sy, FontScale);
                }

                // draw line
                sx = (int)leftMarginEnd;
                for (size_t x = 0; x < textBuff.lines[y]->numCols; ++x) {
                    // text select
                    if ((cursor.selEnd.col != x || cursor.selEnd.ln != y) &&
                        isBetween(y, x, cursor.selBegin.ln, cursor.selBegin.col, cursor.selEnd.ln, cursor.selEnd.col))
                    {
                        SDL_Rect const r = {
                            .x = sx,
                            .y = sy,
                            .w = charWidth,
                            .h = charHeight,
                        };
                        SDL_CHECK_CODE(SDL_RenderFillRect(renderer, &r));
                    }
                    
                    // draw character
                    char ch = textBuff.lines[y]->buff[x];
                    if (ASCII_PRINTABLE_MIN <= ch && ch <= ASCII_PRINTABLE_MAX) {
                        RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, ch, sx, sy, FontScale);
                    }
                    else {
                        // else handle non printable chars
                        assert(0 && "unrenderable character");
                    }
                    sx += charWidth;
                }
            }

            // draw cursor
            SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteR1), 255)); // cursor
            if (cursor.curPos.ln >= topLine && cursor.curPos.ln < topLine+sRows) {
                SDL_Rect const r = {
                    .x = (int)(cursor.curPos.col * charWidth + leftMarginEnd),
                    .y = (int)((cursor.curPos.ln-topLine) * charHeight),
                    .w = 2,
                    .h = charHeight,
                };
                SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
            }

            // draw selection cursor
            if (hasSelection(&cursor) && cursor.curSel.ln >= topLine && cursor.curSel.ln < topLine+sRows) {
                SDL_Rect const r = {
                    .x = (int)(cursor.curSel.col * charWidth + leftMarginEnd),
                    .y = (int)((cursor.curSel.ln-topLine) * charHeight),
                    .w = 2,
                    .h = charHeight,
                };
                SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
            }

            SDL_RenderPresent(renderer);
        }
        else {
            // sleep if necessary
            Uint32 endTick = SDL_GetTicks();
            Uint32 ticksElapsed = endTick - startTick;
            if (ticksElapsed < timestep) {
                SDL_Delay(timestep - ticksElapsed);
            }
        }

    }
    TextBufferFree(textBuff);
    return 0;
}
