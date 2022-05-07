#include "textbuffer.h"
#include <stdbool.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "error.h"
#include <SDL2/SDL.h>

#include "font.h"


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
    LineBuffer** lb = tb->lines + *cursorLn;
    const char* s = event->text;
    size_t n = strlen(s);
    LineBufferInsert(lb, s, n, *cursorCol);
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
        LineBufferInsert(tb->lines + *cursorLn + 1,
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
        LineBufferInsert(tb->lines+*cursorLn, "\t", 1, *cursorCol);
        *cursorCol += 1;
        *cursorColMax = *cursorCol;
    }
    else if (code == SDLK_BACKSPACE) {
        if (*cursorCol >= 1) {
            *cursorCol -= 1;
            LineBufferErase(tb->lines+*cursorLn, 1, *cursorCol);
        }
        else if (*cursorLn != 0) {
            size_t oldCols = tb->lines[*cursorLn-1]->numCols;
            LineBufferInsert(tb->lines+*cursorLn-1,
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
            LineBufferInsert(tb->lines+*cursorLn,
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

static void UpdateScreenSize(SDL_Renderer* renderer, int fontCharWidth, int fontCharHeight, size_t* sRows, size_t* sCols) {
    int sw, sh;
    SDL_GetRendererOutputSize(renderer, &sw, &sh);
    *sRows = sh / fontCharHeight;
    *sCols = sw / fontCharWidth;
}


int main(void) {
    SDL_CHECK_CODE(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* window = SDL_CHECK_PTR(
        SDL_CreateWindow("Editor", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE));

    SDL_Renderer* renderer = SDL_CHECK_PTR(
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    int imgWidth, imgHeight, imgComps;
    unsigned char* imgData = STBI_CHECK_PTR(
        stbi_load("FSEX300.png", &imgWidth, &imgHeight, &imgComps, STBI_rgb_alpha));

    
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        Uint32 rmask = 0xff000000,
            gmask = 0x00ff0000,
            bmask = 0x0000ff00,
            amask = 0x000000ff;
    #else // little endian, like x86
        Uint32 rmask = 0x000000ff,
            gmask = 0x0000ff00,
            bmask = 0x00ff0000,
            amask = 0xff000000;
    #endif

    int depth = 32;
    int pitch = 4 * imgWidth;

    SDL_Surface* fontSurface = STBI_CHECK_PTR(
        SDL_CreateRGBSurfaceFrom((void*)imgData, imgWidth, imgHeight,
            depth, pitch,
            rmask, gmask, bmask, amask));
    
    SDL_Texture* fontTexture = SDL_CHECK_PTR(
        SDL_CreateTextureFromSurface(renderer, fontSurface));
    
    SDL_FreeSurface(fontSurface);

    const float fontScl = 2.0f;
    int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
    int fontCharHeight = imgHeight;
    size_t cursorColMax = 0, cursorCol = 0, cursorLn = 0, topLine = 0;
    TextBuffer textBuff = TextBufferNew(8);
    size_t sRows, sCols;
    UpdateScreenSize(renderer, fontCharWidth*fontScl, fontCharHeight*fontScl, &sRows, &sCols);

    for (bool quit = false; !quit;) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
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
                    UpdateScreenSize(renderer, fontCharWidth*fontScl, fontCharHeight*fontScl, &sRows, &sCols);
                }
            } break;

            case SDL_TEXTINPUT: {
                HandleTextInput(&textBuff, &cursorColMax, &cursorCol, &cursorLn, &e.text);
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(&textBuff, &cursorColMax, &cursorCol, &cursorLn, &e.key);
            } break;
        }

        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteBG), 255));
        SDL_CHECK_CODE(SDL_RenderClear(renderer));


        // numLines should never be zero so log is fine
        int margin = fontCharWidth * fontScl / 2;
        int leftColumn = margin + ((int)log10(textBuff.numLines)+1) * fontCharWidth * fontScl;
        // this loop will change when syntax highlighting is added
        SDL_CHECK_CODE(SDL_SetTextureColorMod(fontTexture, COL_RGB(PaletteW1))); // cursor
        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteFG), 255)); // font

        // TODO: cursor autoscroll

        for (size_t y = topLine;
            y <= topLine+sRows && y < textBuff.numLines;
            ++y)
        {
            int sx, sy = (y-topLine) * fontCharHeight * fontScl;

            // draw line number
            sx = leftColumn;
            int lineNum = y+1;
            while (lineNum != 0) {
                sx -= fontCharWidth * fontScl;
                char d = lineNum % 10 + '0';
                lineNum /= 10;
                RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, d, sx, sy, fontScl);
            }

            // draw line
            sx = leftColumn + margin*2;
            size_t N = textBuff.lines[y]->numCols;
            for (size_t x = 0;; ++x) {
                // draw cursor
                if (y == cursorLn && x == cursorCol) {
                    SDL_Rect r = {.x=sx, .y=sy, .w=2, .h=fontCharHeight*fontScl};
                    SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
                }

                if (x >= N)
                    break;

                // draw character
                char ch = textBuff.lines[y]->buff[x];
                if (ASCII_PRINTABLE_MIN <= ch && ch <= ASCII_PRINTABLE_MAX) {
                    RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, ch, sx, sy, fontScl);
                }
                // else handle non printable chars
                int cw = (1 + (ch == '\t') * (TAB_SIZE-1));
                sx += fontCharWidth * fontScl * cw;
            }
        }
        
        SDL_RenderPresent(renderer);
    }
    TextBufferFree(textBuff);
}
