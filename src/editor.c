#include "textbuffer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "error.h"
#include <SDL2/SDL.h>

#include "config.h"


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

static void HandleTextInput(TextBuffer* tb, size_t* cursorColMax, size_t* cursorCol, size_t* cursorLn, const SDL_TextInputEvent* event) {
    const char* s = event->text;
    size_t n = strlen(s);
    LineBufferInsertStr(tb->lines+*cursorLn, s, n, *cursorCol);
    *cursorCol += n;
    *cursorColMax = *cursorCol;
}

static void HandleKeyDown(TextBuffer* tb, size_t* cursorColMax, size_t* cursorCol, size_t* cursorLn, const SDL_KeyboardEvent* event) {
    // TODO: cursor, text selection, clipboard, zoom, undo/redo, line move, multi-cursor
    // keys: backspace, delete, enter, home, end, pgup, pgdown, left/right, up/down, tab
    // mods: ctrl, shift, alt

    // SDL_Keymod mod = e.key.keysym.mod;
    // TODO: mod and text selection will add second position to consider
    // TODO: slight optimizations, when splitting the current line, choose smaller half to reinsert
    // do this for enter, backspace, delete
    SDL_Keycode code = event->keysym.sym;
    if (code == SDLK_RETURN) {
        TextBufferInsert(tb, 1, *cursorLn + 1);
        LineBufferInsertStr(tb->lines + *cursorLn + 1,
            tb->lines[*cursorLn]->buff + *cursorCol,
            tb->lines[*cursorLn]->numCols - *cursorCol,
            0);
        LineBufferErase(tb->lines + *cursorLn,
            tb->lines[*cursorLn]->numCols - *cursorCol,
            *cursorCol);
        *cursorCol = 0;
        *cursorLn += 1;
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_TAB) {
        LineBufferInsertChr(tb->lines+*cursorLn, ' ', TabSize, *cursorCol);
        *cursorCol += TabSize;
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_BACKSPACE) {
        if (*cursorCol >= 1) {
            *cursorCol -= 1;
            LineBufferErase(tb->lines+*cursorLn, 1, *cursorCol);
        }
        else if (*cursorLn != 0) {
            size_t oldCols = tb->lines[*cursorLn-1]->numCols;
            LineBufferInsertStr(tb->lines+*cursorLn-1,
                tb->lines[*cursorLn]->buff + *cursorCol,
                tb->lines[*cursorLn]->numCols - *cursorCol,
                tb->lines[*cursorLn-1]->numCols);
            *cursorCol = oldCols;
            TextBufferErase(tb, 1, *cursorLn);
            *cursorLn -= 1;
        }
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_DELETE) {
        if (*cursorCol + 1 <= tb->lines[*cursorLn]->numCols) {
            LineBufferErase(tb->lines+*cursorLn, 1, *cursorCol);
        }
        else if (*cursorLn != tb->numLines-1) {
            LineBufferInsertStr(tb->lines+*cursorLn,
                tb->lines[*cursorLn+1]->buff,
                tb->lines[*cursorLn+1]->numCols,
                tb->lines[*cursorLn]->numCols);
            TextBufferErase(tb, 1, *cursorLn+1);
        }
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_LEFT) {
        if (*cursorCol >= 1) {
            *cursorCol -= 1;
        }
        else if (*cursorLn >= 1) {
            *cursorLn -= 1;
            *cursorCol = tb->lines[*cursorLn]->numCols;
        }
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_RIGHT) {
        if (*cursorCol + 1 <= tb->lines[*cursorLn]->numCols) {
            *cursorCol += 1;
        }
        else if (*cursorLn + 1 < tb->numLines) {
            *cursorCol = 0;
            *cursorLn += 1;
        }
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_UP) {
        if (*cursorLn >= 1) {
            *cursorLn -= 1;
            if (*cursorCol < *cursorColMax)
                *cursorCol = *cursorColMax;
            size_t cols = tb->lines[*cursorLn]->numCols;
            if (*cursorCol > cols)
                *cursorCol = cols;
        }
        else {
            *cursorLn = 0;
            *cursorCol = 0;
            *cursorColMax = *cursorCol;
        }
    }
    else if (code == SDLK_DOWN) {
        if (*cursorLn + 1 < tb->numLines) {
            *cursorLn += 1;
            if (*cursorCol < *cursorColMax)
                *cursorCol = *cursorColMax;
            size_t cols = tb->lines[*cursorLn]->numCols;
            if (*cursorCol > cols)
                *cursorCol = cols;
        }
        else {
            *cursorLn = tb->numLines-1;
            *cursorCol = tb->lines[*cursorLn]->numCols;
            *cursorColMax = *cursorCol;
        }
    }
    else if (code == SDLK_END) {
        *cursorCol = tb->lines[*cursorLn]->numCols;
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_HOME) {
        *cursorCol = 0;
        *cursorColMax = *cursorCol;
    }
}

static void GetScreenSize(SDL_Renderer* renderer, int charWidth, int charHeight, size_t* sRows, size_t* sCols) {
    int sw, sh;
    SDL_GetRendererOutputSize(renderer, &sw, &sh);
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

static void ScreenToCursor(size_t* cursorX, size_t* cursorY, size_t mouseX, size_t mouseY, TextBuffer const* textBuff, size_t leftMarginEnd, int charWidth, int charHeight) {
    // offset due to line numbers
    if (mouseX < leftMarginEnd) mouseX = 0;
    else mouseX -= leftMarginEnd;
    // round forward or back to nearest char
    mouseX += charWidth / 2;

    *cursorX = mouseX / charWidth;
    *cursorY = mouseY / charHeight;

    // clamp y
    if (*cursorY > textBuff->numLines-1)
        *cursorY = textBuff->numLines-1;

    // clamp x
    size_t maxCols = textBuff->lines[*cursorY]->numCols;
    if (*cursorX > maxCols)
        *cursorX = maxCols;
}

static bool lexLe(size_t y0, size_t x0, size_t y1, size_t x1) {
    // (y0,x0) <=_lex (y1,x1)
    return (y0 < y1) || (y0 == y1 && x0 <= x1);
}

static bool isBetween(size_t ln, size_t col, size_t y0, size_t x0, size_t y1, size_t x1) {
    // (y0,x0) <= (ln,col) <= (y1,x1)
    return lexLe(y0, x0, ln, col) && lexLe(ln, col, y1, x1);
}

static void UpdateSelection(
    size_t* selectionBeginCol, size_t* selectionBeginLn, size_t* selectionEndCol, size_t* selectionEndLn,
    size_t cursorLn, size_t cursorCol, size_t selectionLn, size_t selectionCol
) {
    if (lexLe(cursorLn, cursorCol, selectionLn, selectionCol)) {
        *selectionBeginCol = cursorCol;
        *selectionBeginLn = cursorLn;
        *selectionEndCol = selectionCol;
        *selectionEndLn = selectionLn;
    }
    else {
        *selectionBeginCol = selectionCol;
        *selectionBeginLn = selectionLn;
        *selectionEndCol = cursorCol;
        *selectionEndLn = cursorLn;
    }
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
    size_t cursorCol = 0, cursorLn = 0, cursorColMax = 0, topLine = 0,
        selectionCol = 0, selectionLn = 0, selectionBeginCol = 0, selectionBeginLn = 0, selectionEndCol = 0, selectionEndLn = 0,
        sRows, sCols, leftMarginBegin, leftMarginEnd;
    bool mouseHeld = false;
    GetScreenSize(renderer, charWidth, charHeight, &sRows, &sCols);
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
                if (mouseHeld) {
                    ScreenToCursor(&cursorCol, &cursorLn, e.button.x, e.button.y, &textBuff, leftMarginEnd, charWidth, charHeight);
                    UpdateSelection(&selectionBeginCol, &selectionBeginLn, &selectionEndCol, &selectionEndLn, cursorLn, cursorCol, selectionLn, selectionCol);
                }

            } break;

            case SDL_MOUSEBUTTONDOWN: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseHeld = true;
                    assert(e.button.x >= 0);
                    assert(e.button.y >= 0);

                    ScreenToCursor(&cursorCol, &cursorLn, e.button.x, e.button.y, &textBuff, leftMarginEnd, charWidth, charHeight);
                    cursorColMax = cursorCol;

                    selectionCol = cursorCol;
                    selectionLn = cursorLn;
                    UpdateSelection(&selectionBeginCol, &selectionBeginLn, &selectionEndCol, &selectionEndLn, cursorLn, cursorCol, selectionLn, selectionCol);
                }
            } break;

            case SDL_MOUSEBUTTONUP: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseHeld = false;
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
                    GetScreenSize(renderer, charWidth, charHeight, &sRows, &sCols);
                }
            } break;

            case SDL_TEXTINPUT: {
                HandleTextInput(&textBuff, &cursorColMax, &cursorCol, &cursorLn, &e.text);
                CursorAutoscroll(&topLine, cursorLn, sRows);
                selectionCol = cursorCol;
                selectionLn = cursorLn;
                UpdateSelection(&selectionBeginCol, &selectionBeginLn, &selectionEndCol, &selectionEndLn, cursorLn, cursorCol, selectionLn, selectionCol);
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(&textBuff, &cursorColMax, &cursorCol, &cursorLn, &e.key);
                CursorAutoscroll(&topLine, cursorLn, sRows);
                // on enter or delete, number of lines can change -> recalculate margin
                CalculateLeftMargin(&leftMarginBegin, &leftMarginEnd, charWidth, textBuff.numLines);
                selectionCol = cursorCol;
                selectionLn = cursorLn;
                UpdateSelection(&selectionBeginCol, &selectionBeginLn, &selectionEndCol, &selectionEndLn, cursorLn, cursorCol, selectionLn, selectionCol);
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
                if ((selectionEndCol != x || selectionEndLn != y) &&
                    isBetween(y, x, selectionBeginLn, selectionBeginCol, selectionEndLn, selectionEndCol))
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
        if (cursorLn >= topLine && cursorLn < topLine+sRows) {
            SDL_Rect const r = {
                .x = cursorCol * charWidth + leftMarginEnd,
                .y = (cursorLn-topLine) * charHeight,
                .w = 2,
                .h = charHeight,
            };
            SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
        }

        // draw selection cursor
        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteG1), 255)); // cursor
        if (selectionLn >= topLine && selectionLn < topLine+sRows) {
            SDL_Rect const r = {
                .x = selectionCol * charWidth + leftMarginEnd,
                .y = (selectionLn-topLine) * charHeight,
                .w = 2,
                .h = charHeight,
            };
            SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
        }

        SDL_RenderPresent(renderer);
    }
    TextBufferFree(textBuff);
}
