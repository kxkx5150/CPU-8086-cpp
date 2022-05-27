#include "PC.h"
#include "Intel8086.h"
#include "Intel8255.h"
#include "Motorola6845.h"
#include <cstdio>

std::vector<std::vector<uint8_t>> COLORS = {{0, 0, 0},     {0, 0, 170},    {0, 170, 0},    {0, 170, 170},
                                            {170, 0, 0},   {170, 0, 170},  {170, 85, 0},   {170, 170, 170},
                                            {85, 85, 85},  {85, 85, 255},  {85, 255, 85},  {85, 255, 255},
                                            {255, 85, 85}, {255, 85, 255}, {255, 255, 85}, {255, 255, 255}};

PC::PC()
{
    m_cpu = new Intel8086();
    m_cpu->init();

    TTF_Init();
    font = TTF_OpenFont("bin/cp437.ttf", 12);
    if (font == NULL) {
        printf("error: font not found\n");
        exit(EXIT_FAILURE);
    }
}
PC::~PC()
{
    delete m_cpu;
}
void PC::reset()
{
    m_cpu->reset();
}
void PC::run_cpu()
{
    m_cpu->run();
}
void PC::paint(SDL_Renderer *render, int widht, int height)
{
    SDL_RenderSetScale(render, 1, 1);
    const int curAttr = m_cpu->m_crtc->getRegister(0xa) >> 4;
    const int curLoc  = m_cpu->m_crtc->getRegister(0xf) | m_cpu->m_crtc->getRegister(0xe) << 8;

    for (int y = 0; y < 25; ++y) {
        for (int x = 0; x < 80; ++x) {
            const uint16_t character = m_cpu->m_memory[0xb8000 + 2 * (x + y * 80)];
            const uint16_t attribute = m_cpu->m_memory[0xb8000 + 2 * (x + y * 80) + 1];

            auto gbcolor = COLORS[attribute >> 4 & 0b111];
            SDL_SetRenderDrawColor(render, gbcolor[0], gbcolor[1], gbcolor[2], SDL_ALPHA_OPAQUE);
            SDL_Rect rect;
            rect.x = x * 7;
            rect.y = y * 12;
            rect.w = 7;
            rect.h = 12;
            SDL_RenderFillRect(render, &rect);

            // font
            if (32 < character && character < 127 && character != 85) {
                // printf("%c\n", character);
            }
            // auto fntcolor  = COLORS[attribute & 0b1111];
            // SDL_Color color={fntcolor[0],fntcolor[1],fntcolor[2]};
            // SDL_Surface* text_surface = TTF_RenderUNICODE_Solid(font, &chr, color);
            // SDL_Texture* Message = SDL_CreateTextureFromSurface(render, text_surface);
            // SDL_Rect rectfg;
            // rectfg.x = x *7;
            // rectfg.y = y *12;
            // rectfg.w = 7;
            // rectfg.h = 12;
            // SDL_RenderCopy(render, Message, NULL, &rectfg);
            // SDL_FreeSurface(text_surface);
            // SDL_DestroyTexture(Message);
        }
    }

    // SDL_RenderClear(render);
    SDL_RenderPresent(render);
}
