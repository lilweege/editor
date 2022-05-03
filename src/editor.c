#include <stdbool.h>

#include "SDL_helper.h"
#include <SDL2/SDL.h>


int main(void) {
    SDL_CHECK_CODE(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* window = SDL_CHECK_PTR(
        SDL_CreateWindow("Editor", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE));

    SDL_Renderer* renderer = SDL_CHECK_PTR(
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    for (bool quit = false; !quit;) {
        SDL_Event e = {0};
        while (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT: {
                quit = true;
            } break;
        }

        SDL_CHECK_CODE(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0));
        SDL_CHECK_CODE(SDL_RenderClear(renderer));
        SDL_RenderPresent(renderer);
    }
}
