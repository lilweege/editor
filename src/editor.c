#include "textbuffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>

#include "error.h"
#include "config.h"


typedef struct {
    size_t ln, col;
} CursorPos;

typedef struct {
    CursorPos curPos, curSel;
    CursorPos selBegin, selEnd;
    size_t colMax;
    bool shiftSelecting, mouseSelecting;
} Cursor;


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

static void EraseBetween(TextBuffer* tb, CursorPos begin, CursorPos end) {
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
}

static void EraseSelection(TextBuffer* tb, Cursor* cursor) {
    EraseBetween(tb, cursor->selBegin, cursor->selEnd);
    cursor->curPos.ln = cursor->selBegin.ln;
    size_t cols = tb->lines[cursor->curPos.ln]->numCols;
    cursor->curPos.col = cursor->selBegin.col;
    if (cursor->curPos.col > cols)
        cursor->curPos.col = cols;
    StopSelecting(cursor);
}

static void RenderCharacter(SDL_Renderer* renderer, SDL_Texture* fontTexture, int fontCharWidth, int fontCharHeight, char ch, int x, int y, float scl) {
    size_t idx = ch - ASCII_PRINTABLE_MIN;
    SDL_Rect sourceRect = {
        .x = idx*fontCharWidth,
        .y = 0,
        .w = fontCharWidth,
        .h = fontCharHeight,
    };

    SDL_Rect screenRect = {
        .x = x,
        .y = y,
        .w = fontCharWidth * scl,
        .h = fontCharHeight * scl,
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
    LineBufferInsertStr(tb->lines+cursor->curPos.ln, s, n, cursor->curPos.col);
    cursor->curPos.col += n;
    cursor->colMax = cursor->curPos.col;
}

static void HandleKeyDown(TextBuffer* tb, Cursor* cursor, SDL_KeyboardEvent const* event) {
    // TODO: cursor, text selection, clipboard, zoom, undo/redo, line move, multi-cursor
    // keys: backspace, delete, enter, home, end, pgup, pgdown, left/right, up/down, tab
    // mods: ctrl, shift, alt

    SDL_Keycode code = event->keysym.sym;
    SDL_Keymod mod = event->keysym.mod;
    // TODO: mod and text selection will add second position to consider
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
    {
        // directional keys
        // TODO: mod & KMOD_CTRL
        if (hasSelection(cursor))
            cursor->shiftSelecting = true;
        bool wasSelecting = cursor->shiftSelecting;
        if (mod & KMOD_SHIFT) {
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
    *leftMarginBegin = LineMarginLeft * charWidth + ((int)log10(numLines)+1) * charWidth;
    *leftMarginEnd = *leftMarginBegin + LineMarginRight * charWidth;
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


int main(void) {
    SDL_CHECK_CODE(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* const window = SDL_CHECK_PTR(
        SDL_CreateWindow(ProgramTitle, 0, 0, InitialWindowWidth, InitialWindowHeight, SDL_WINDOW_RESIZABLE));
    SDL_Renderer* const renderer = SDL_CHECK_PTR(
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    // image -> surface -> texture
    int imgWidth, imgHeight, imgComps;
    unsigned char const* const imgData = STBI_CHECK_PTR(
        stbi_load(FontFilename, &imgWidth, &imgHeight, &imgComps, STBI_rgb_alpha));
    
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
    int const charWidth = fontCharWidth * FontScale;
    int const charHeight = fontCharHeight * FontScale;
    TextBuffer textBuff = TextBufferNew(8);
    Cursor cursor = {0};
    size_t topLine = 0, sRows, sCols, leftMarginBegin, leftMarginEnd;

    GetScreenSize(window, charWidth, charHeight, &sRows, &sCols);
    CalculateLeftMargin(&leftMarginBegin, &leftMarginEnd, charWidth, textBuff.numLines);

    // main loop
    // TODO: yield thread, don't pin cpu
    for (bool quit = false; !quit;) {
        // handle events
        SDL_Event e;
        if (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
            } break;

            case SDL_MOUSEMOTION: {
                // update cursor style
                if (currentMouseCursor != mouseCursorIBeam && (size_t)e.motion.x > leftMarginEnd) {
                    currentMouseCursor = mouseCursorIBeam;
                    SDL_SetCursor(mouseCursorIBeam);
                }
                else if (currentMouseCursor != mouseCursorArrow && (size_t)e.motion.x <= leftMarginEnd) {
                    currentMouseCursor = mouseCursorArrow;
                    SDL_SetCursor(mouseCursorArrow);
                }
                
                // mouse drag selection
                if (cursor.mouseSelecting) {
                    ScreenToCursor(&cursor, e.button.x, e.button.y, &textBuff, leftMarginEnd, topLine, charWidth, charHeight);
                    UpdateSelection(&cursor);
                }

            } break;

            case SDL_MOUSEBUTTONDOWN: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cursor.mouseSelecting = true;
                    assert(e.button.x >= 0);
                    assert(e.button.y >= 0);

                    ScreenToCursor(&cursor, e.button.x, e.button.y, &textBuff, leftMarginEnd, topLine, charWidth, charHeight);
                    cursor.colMax = cursor.curPos.col;
                    
                    cursor.curSel.col = cursor.curPos.col;
                    cursor.curSel.ln = cursor.curPos.ln;
                    UpdateSelection(&cursor);
                }
            } break;

            case SDL_MOUSEBUTTONUP: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cursor.mouseSelecting = false;
                }
            } break;

            case SDL_MOUSEWHEEL: {
                // TODO: horizontal scrolling
                Sint32 dy = e.wheel.y;
                if ((dy > 0 && topLine >= (size_t)dy) || // up
                    (dy < 0 && topLine + (-dy) < textBuff.numLines)) // down
                {
                    topLine -= dy;
                }
            } break;

            case SDL_WINDOWEVENT: { // https://wiki.libsdl.org/SDL_WindowEvent
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    GetScreenSize(window, charWidth, charHeight, &sRows, &sCols);
                }
            } break;

            case SDL_TEXTINPUT: {
                HandleTextInput(&textBuff, &cursor, &e.text);
                CursorAutoscroll(&topLine, cursor.curPos.ln, sRows);
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(&textBuff, &cursor, &e.key);
                CursorAutoscroll(&topLine, cursor.curPos.ln, sRows);
                // on enter or delete, number of lines can change -> recalculate margin
                CalculateLeftMargin(&leftMarginBegin, &leftMarginEnd, charWidth, textBuff.numLines);
            } break;
        }

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
            size_t const sy = (y-topLine) * charHeight;

            // draw line number
            size_t sx = leftMarginBegin;
            for (size_t lineNum = y+1; lineNum > 0; lineNum /= 10) {
                sx -= charWidth;
                RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, lineNum % 10 + '0', sx, sy, FontScale);
            }

            // draw line
            sx = leftMarginEnd;
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
                // else handle non printable chars
                sx += charWidth;
            }
        }

        // draw cursor
        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteR1), 255)); // cursor
        if (cursor.curPos.ln >= topLine && cursor.curPos.ln < topLine+sRows) {
            SDL_Rect const r = {
                .x = cursor.curPos.col * charWidth + leftMarginEnd,
                .y = (cursor.curPos.ln-topLine) * charHeight,
                .w = 2,
                .h = charHeight,
            };
            SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
        }

        // draw selection cursor
        if (hasSelection(&cursor) && cursor.curSel.ln >= topLine && cursor.curSel.ln < topLine+sRows) {
            SDL_Rect const r = {
                .x = cursor.curSel.col * charWidth + leftMarginEnd,
                .y = (cursor.curSel.ln-topLine) * charHeight,
                .w = 2,
                .h = charHeight,
            };
            SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
        }

        SDL_RenderPresent(renderer);
    }
    TextBufferFree(textBuff);
}
