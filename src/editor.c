#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "SDL_helper.h"
#include <SDL2/SDL.h>

#include "font.h"

void RenderCharacter(SDL_Renderer* renderer, SDL_Texture* fontTexture, int imgWidth, int imgHeight, char ch, int x, int y, float scl) {
    int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
    int fontCharHeight = imgHeight;
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
    SDL_CHECK_CODE(SDL_RenderCopy(renderer, fontTexture, &sourceRect, &screenRect));
}


SDL_Point RenderSString(SDL_Renderer* renderer, SDL_Texture* fontTexture, int imgWidth, int imgHeight, const char* s, size_t n, SDL_Point p, int x0, float scl, Uint32 col) {
    SDL_CHECK_CODE(SDL_SetTextureColorMod(fontTexture, COL_R(col), COL_G(col), COL_B(col)));
    int fontCharWidth = imgWidth / ASCII_PRINTABLE_CNT;
    int fontCharHeight = imgHeight;
    int x = p.x, y = p.y;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        RenderCharacter(renderer, fontTexture, imgWidth, imgHeight, c, x, y, scl);
        if (ASCII_PRINTABLE_MIN <= c && c <= ASCII_PRINTABLE_MAX) {
            x += fontCharWidth*scl;
        }
        else if (c == '\t') {
            x += fontCharWidth*scl * TAB_SIZE;
        }
        else if (c == '\n') {
            x = x0;
            y += fontCharHeight*scl;
        }
        // otherwise, ignore it
    }
    return (SDL_Point) { x, y };
}

SDL_Point RenderCString(SDL_Renderer* renderer, SDL_Texture* fontTexture, int imgWidth, int imgHeight, const char* s, SDL_Point p, int x0, float scl, Uint32 col) {
    return RenderSString(renderer, fontTexture, imgWidth, imgHeight, s, strlen(s), p, x0, scl, col);
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


    for (bool quit = false; !quit;) {
        SDL_Event e = {0};
        while (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
            } break;
        }

        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, COL_R(PaletteBG), COL_G(PaletteBG), COL_B(PaletteBG), 255));
        SDL_CHECK_CODE(SDL_RenderClear(renderer));

        float fontScl = 2.0f;
        int leftMargin = 0;
        SDL_Point cursor = { 0, 0 };
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "#include ", cursor, leftMargin, fontScl, PaletteR1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "<stdio.h>\n\n", cursor, leftMargin, fontScl, PaletteY1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "int ", cursor, leftMargin, fontScl, PaletteC1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "main", cursor, leftMargin, fontScl, PaletteG1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "(", cursor, leftMargin, fontScl, PaletteFG);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "int ", cursor, leftMargin, fontScl, PaletteC1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "argc", cursor, leftMargin, fontScl, PaletteY1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, ", ", cursor, leftMargin, fontScl, PaletteFG);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "char", cursor, leftMargin, fontScl, PaletteC1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "** ", cursor, leftMargin, fontScl, PaletteR1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "argv", cursor, leftMargin, fontScl, PaletteY1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, ") {\n\t", cursor, leftMargin, fontScl, PaletteFG);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "printf", cursor, leftMargin, fontScl, PaletteC1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "(", cursor, leftMargin, fontScl, PaletteFG);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "\"Hello, world!", cursor, leftMargin, fontScl, PaletteY2);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "\\n", cursor, leftMargin, fontScl, PaletteM1);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, "\"", cursor, leftMargin, fontScl, PaletteY2);
        cursor = RenderCString(renderer, fontTexture, imgWidth, imgHeight, ");\n}", cursor, leftMargin, fontScl, PaletteFG);

        SDL_RenderPresent(renderer);
    }
}
