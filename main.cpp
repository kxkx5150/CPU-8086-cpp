#include <cstddef>
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <cstdio>
#include "src/PC.h"
#include <time.h>


int main(int ArgCount, char **Args)
{
    static const int width = 560, height = 300;
    auto    pc         = new PC();
    clock_t beginFrame = clock();
    size_t  frames     = 0;

    SDL_Window *window =
        SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);    
    SDL_Renderer * render = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    for (int Running = 1; Running;) {
        pc->run_cpu();
        clock_t endFrame = clock();
        frames++;
        if (endFrame - beginFrame > 1000.0 / 60) {
            pc->paint(render, width, height);
            beginFrame = endFrame;
            frames     = 0;
        }

        SDL_Event Event;
        while (SDL_PollEvent(&Event)) {
            if (Event.type == SDL_QUIT)
                Running = 0;
        }
    }

    return 0;
}
