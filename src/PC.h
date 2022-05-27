#pragma once
#include <stdexcept>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Intel8086;
class Intel8255;
class Motorola6845;



class PC {
  private:
    Intel8086 *m_cpu = nullptr;
    TTF_Font  *font  = nullptr;

    //
  public:
    PC();
    ~PC();

    void reset();
    void run_cpu();

    void paint(SDL_Renderer *render, int widht, int height);
};
