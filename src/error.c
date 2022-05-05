#include "error.h"
#include "stb_image.h"
#include <SDL2/SDL_error.h>

void SDLError(const char* file, int line) {
    PANIC("SDL", SDL_GetError(), file, line);
}

void STBIError(const char* file, int line) {
    PANIC("STBI", stbi_failure_reason(), file, line);
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
