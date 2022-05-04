#include <SDL2/SDL_keycode.h>
#include <stdbool.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "SDL_helper.h"
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


static void RenderSString(SDL_Renderer* renderer, SDL_Texture* fontTexture, int imgWidth, int imgHeight, const char* s, size_t n, SDL_Point* p, int x0, float scl, Uint32 col) {
    SDL_CHECK_CODE(SDL_SetTextureColorMod(fontTexture, COL_R(col), COL_G(col), COL_B(col)));
    int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
    int fontCharHeight = imgHeight;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        RenderCharacter(renderer, fontTexture, fontCharWidth, fontCharHeight, c, p->x, p->y, scl);
        if (ASCII_PRINTABLE_MIN <= c && c <= ASCII_PRINTABLE_MAX) {
            p->x += fontCharWidth*scl;
        }
        else if (c == '\t') {
            p->x += fontCharWidth*scl * TAB_SIZE;
        }
        else if (c == '\n') {
            p->x = x0;
            p->y += fontCharHeight*scl;
        }
        // otherwise, ignore it
    }
}

static void RenderCString(SDL_Renderer* renderer, SDL_Texture* fontTexture, int imgWidth, int imgHeight, const char* s, SDL_Point* p, int x0, float scl, Uint32 col) {
    RenderSString(renderer, fontTexture, imgWidth, imgHeight, s, strlen(s), p, x0, scl, col);
}


static void HandleTextInput(char* txtBuff, const size_t txtBuffCap, size_t* txtBuffSzP, size_t* txtCursorPosP, const SDL_TextInputEvent* event) {
    const size_t txtBuffSz = *txtBuffSzP, txtCursorPos = *txtCursorPosP;
    const char* s = event->text;
    size_t n = strlen(s);
    if (txtBuffSz + n >= txtBuffCap)
        return;
    memmove(txtBuff+txtCursorPos+n, txtBuff+txtCursorPos, txtBuffSz-txtCursorPos);
    memcpy(txtBuff+txtCursorPos, s, n);
    *txtBuffSzP += n;
    *txtCursorPosP += n;
}

static void HandleKeyDown(char* txtBuff, const size_t txtBuffCap, size_t* txtBuffSzP, size_t* txtCursorPosP, const SDL_KeyboardEvent* event) {
    const size_t txtBuffSz = *txtBuffSzP, txtCursorPos = *txtCursorPosP;
    // TODO: cursor, text selection, clipboard, zoom, undo/redo, line move, multi-cursor
    // keys: backspace, delete, enter, home, end, pgup, pgdown, left/right, up/down, tab
    // mods: ctrl, shift, alt
    
    // SDL_Keymod mod = e.key.keysym.mod;
    size_t n = 1;
    SDL_Keycode code = event->keysym.sym;
    if (code == SDLK_BACKSPACE) {
        if (txtCursorPos >= n) {
            memmove(txtBuff+txtCursorPos-n, txtBuff+txtCursorPos, txtBuffSz-txtCursorPos);
            *txtBuffSzP -= n;
            *txtCursorPosP -= n;
        }
    }
    else if (code == SDLK_DELETE) {
        if (txtCursorPos + n <= txtBuffSz) {
            memmove(txtBuff+txtCursorPos, txtBuff+txtCursorPos+n, txtBuffSz-txtCursorPos-n);
            *txtBuffSzP -= n;
        }
    }
    else if (code == SDLK_LEFT) {
        if (txtCursorPos >= n) {
            *txtCursorPosP -= n;
        }
    }
    else if (code == SDLK_RIGHT) {
        if (txtCursorPos + n <= txtBuffSz) {
            *txtCursorPosP += n;
        }
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

    size_t txtBuffSz = 0;
    size_t txtCursorPos = 0;
    const size_t txtBuffCap = 1024;
    char txtBuff[txtBuffCap];
    memset(txtBuff, '_', 1023);


    for (bool quit = false; !quit;) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
            } break;

            case SDL_TEXTINPUT: {
                HandleTextInput(txtBuff, txtBuffCap, &txtBuffSz, &txtCursorPos, &e.text);
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(txtBuff, txtBuffCap, &txtBuffSz, &txtCursorPos, &e.key);
            } break;
        }

        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteBG), 255));
        SDL_CHECK_CODE(SDL_RenderClear(renderer));

        const float fontScl = 2.0f;
        const int leftMargin = 0;
        SDL_Point cursor = { 0, 0 };
        RenderSString(renderer, fontTexture, imgWidth, imgHeight, txtBuff, txtCursorPos, &cursor, leftMargin, fontScl, PaletteFG);
        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_RGB(PaletteFG), 255));

        int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
        int fontCharHeight = imgHeight;
        // line cursor
        // SDL_Rect r = {.x=cursor.x, .y=cursor.y, .w=2, .h=fontCharHeight*fontScl};
        // SDL_CHECK_CODE(SDL_RenderFillRect(renderer, &r));
        // block cursor
        SDL_Rect r = {.x=cursor.x, .y=cursor.y, .w=fontCharWidth*fontScl, .h=fontCharHeight*fontScl};
        SDL_CHECK_CODE(SDL_RenderDrawRect(renderer, &r));

        RenderSString(renderer, fontTexture, imgWidth, imgHeight, txtBuff+txtCursorPos, txtBuffSz-txtCursorPos, &cursor, leftMargin, fontScl, PaletteFG);

        
        SDL_RenderPresent(renderer);
    }
}
