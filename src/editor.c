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

    size_t cursorColMax = 0, cursorCol = 0, cursorLn = 0;
    TextBuffer textBuff = TextBufferNew(8);

    for (bool quit = false; !quit;) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
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

        const float fontScl = 2.0f;
        int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
        int fontCharHeight = imgHeight;
        
        // // debug, doesn't handle tabs
        // for (size_t y = 0; y < textBuff.numLines; ++y) {
        //     SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(0xFF0000), 255));
        //     for (size_t x = 0; x < textBuff.lines[y]->maxCols; ++x) {
        //         if (x == textBuff.lines[y]->numCols)
        //             SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(0xFFFF00), 255));
        //         SDL_Rect cell = {.x=fontCharWidth*fontScl*x, .y=fontCharHeight*fontScl*y, .w=fontCharWidth*fontScl, .h=fontCharHeight*fontScl};
        //         SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &cell));
        //     }
        // }
        // SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(0xFF00FF), 255));
        // for (size_t y = 0; y < textBuff.numLines; ++y) {
        //     SDL_Rect cell = {.x=textBuff.lines[y]->numCols*fontCharWidth*fontScl, .y=y*fontCharHeight*fontScl, .w=4, .h=fontCharHeight*fontScl};
        //     SDL_CHECK_CODE(SDL_RenderFillRect(renderer, &cell));
        // }
        // SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(0x00FF00), 255));
        // for (size_t y = 0; y < textBuff.maxLines; ++y) {
        //     size_t x = (y < textBuff.numLines) ? textBuff.lines[y]->numCols : 0;
        //     SDL_Rect cell = {.x=x*fontCharWidth*fontScl+4, .y=y*fontCharHeight*fontScl, .w=4, .h=fontCharHeight*fontScl};
        //     SDL_CHECK_CODE(SDL_RenderFillRect(renderer, &cell));
        // }

        // this loop will change when syntax highlighting is added
        SDL_CHECK_CODE(SDL_SetTextureColorMod(fontTexture, COL_RGB(PaletteW1))); // cursor
        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteFG), 255)); // font
        for (size_t y = 0; y < textBuff.numLines; ++y) {
            size_t sx = 0, sy = y * fontCharHeight * fontScl;
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
