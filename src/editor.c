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

static void HandleTextInput(TextBuffer* tb, size_t* cursorCol, size_t* cursorLn, const SDL_TextInputEvent* event) {
    LineBuffer** lb = tb->lines + *cursorLn;
    const char* s = event->text;
    size_t n = strlen(s);
    LineBufferInsert(lb, s, n, *cursorCol);
    *cursorCol += n;
}

static void HandleKeyDown(TextBuffer* tb, size_t* cursorCol, size_t* cursorLn, const SDL_KeyboardEvent* event) {
    LineBuffer** lb = tb->lines + *cursorLn;
    // TODO: cursor, text selection, clipboard, zoom, undo/redo, line move, multi-cursor
    // keys: backspace, delete, enter, home, end, pgup, pgdown, left/right, up/down, tab
    // mods: ctrl, shift, alt
    
    // SDL_Keymod mod = e.key.keysym.mod;
    size_t n = 1; // mod change this
    SDL_Keycode code = event->keysym.sym;
    if (code == SDLK_RETURN) {
        TextBufferInsert(tb, tb->numLines);
        *cursorLn += 1;
        *cursorCol = 0;
    }
    if (code == SDLK_TAB) {
        LineBufferInsert(lb, "\t", 1, *cursorCol);
        *cursorCol += n;
    }
    else if (code == SDLK_BACKSPACE) {
        if (*cursorCol >= n) {
            *cursorCol -= n;
            LineBufferErase(lb, n, *cursorCol);
        }
    }
    else if (code == SDLK_DELETE) {
        if (*cursorCol + n <= (*lb)->numCols) {
            LineBufferErase(lb, n, *cursorCol);
        }
    }
    else if (code == SDLK_LEFT) {
        if (*cursorCol >= n) {
            *cursorCol -= n;
        }
    }
    else if (code == SDLK_RIGHT) {
        if (*cursorCol + n <= (*lb)->numCols) {
            *cursorCol += n;
        }
    }
    else if (code == SDLK_UP) {
        *cursorLn -= 1;
    }
    else if (code == SDLK_DOWN) {
        *cursorLn += 1;
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

    size_t cursorCol = 0, cursorLn = 0;
    TextBuffer textBuff = TextBufferNew(8);

    for (bool quit = false; !quit;) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
            } break;

            case SDL_TEXTINPUT: {
                HandleTextInput(&textBuff, &cursorCol, &cursorLn, &e.text);
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(&textBuff, &cursorCol, &cursorLn, &e.key);
            } break;
        }

        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteBG), 255));
        SDL_CHECK_CODE(SDL_RenderClear(renderer));

        const float fontScl = 2.0f;
        int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
        int fontCharHeight = imgHeight;
        
        SDL_CHECK_CODE(SDL_SetTextureColorMod(fontTexture, COL_RGB(0xFFFFFF)));
        for (size_t sx = 0, sy = 0, y = 0; y < textBuff.numLines; ++y) {
            for (size_t x = 0; x < textBuff.lines[y]->numCols; ++x) {
                char ch = textBuff.lines[y]->buff[x];
                if (ASCII_PRINTABLE_MIN <= ch && ch <= ASCII_PRINTABLE_MAX) {
                    RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, ch, sx, sy, fontScl);
                    sx += fontCharWidth*fontScl;
                }
                else if (ch == '\t') {
                    sx += fontCharWidth*fontScl * TAB_SIZE;
                }
                // otherwise, ignore it
            }
            sx = 0;
            sy += fontCharHeight*fontScl;
        }

        // block cursor
        SDL_Rect r = {.x=cursorCol*fontCharWidth*fontScl, .y=cursorLn*fontCharHeight*fontScl, .w=fontCharWidth*fontScl, .h=fontCharHeight*fontScl};
        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteFG), 255));
        SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));
        
        SDL_RenderPresent(renderer);
    }
}
