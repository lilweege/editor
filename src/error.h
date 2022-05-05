#ifndef ERROR_H_
#define ERROR_H_

#include <stdio.h>

#define PANIC_HERE(kind, msg) PANIC(kind, msg, __FILE__, __LINE__)
#define PANIC(kind, msg, file, line) (fprintf(stderr, "%s:%d: " kind " ERROR: %s\n", file, line, msg), exit(1))
#define SDL_CHECK_CODE(x) SDLCheckCode(x, __FILE__, __LINE__)
#define SDL_CHECK_PTR(p) SDLCheckPtr(p, __FILE__, __LINE__)
#define STBI_CHECK_PTR(p) STBICheckPtr(p, __FILE__, __LINE__)

void SDLError(const char* file, int line);
int SDLCheckCode(int code, const char* file, int line);
void* SDLCheckPtr(void* ptr, const char* file, int line);
void* STBICheckPtr(void* ptr, const char* file, int line);

#endif // ERROR_H_
