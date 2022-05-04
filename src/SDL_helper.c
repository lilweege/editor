#include "SDL_helper.h"
#include "stb_image.h"
#include <stdio.h>
#include <SDL2/SDL_error.h>

void SDLError(const char* file, int line) {
    fprintf(stderr, "%s:%d: SDL ERROR: %s",
        file, line, SDL_GetError());
    exit(1);
}

void STBIError(const char* file, int line) {
    fprintf(stderr, "%s:%d: SDL ERROR: %s\n",
        file, line, stbi_failure_reason());
    exit(1);
}

int SDLCheckCode(int code, const char* file, int line) {
    if (code < 0)
        SDLError(file, line);
    return code;
}

void* SDLCheckPtr(void* ptr, const char* file, int line) {
    if (ptr == NULL)
        SDLError(file, line);
    return ptr;
}

void* STBICheckPtr(void* ptr, const char* file, int line) {
    if (ptr == NULL)
        STBIError(file, line);
    return ptr;
}
