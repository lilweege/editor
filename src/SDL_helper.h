#ifndef SDL_HELPER_H_
#define SDL_HELPER_H_

#define SDL_CHECK_CODE(x) SDLCheckCode(x, __FILE__, __LINE__)
#define SDL_CHECK_PTR(p) SDLCheckPtr(p, __FILE__, __LINE__)

void SDLError(const char* file, int line);
int SDLCheckCode(int code, const char* file, int line);
void* SDLCheckPtr(void* ptr, const char* file, int line);

#endif // SDL_HELPER_H_
