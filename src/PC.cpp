#include "PC.h"
#include "Intel8086.h"
#include "Intel8255.h"
#include "Motorola6845.h"
#include <cstdio>

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
void PC::paint(SDL_Renderer *render, int widht , int height)
{
    SDL_RenderSetScale(render, 1, 1);
    const int curAttr = m_cpu->m_crtc->getRegister(0xa) >> 4;
    const int curLoc  = m_cpu->m_crtc->getRegister(0xf) | m_cpu->m_crtc->getRegister(0xe) << 8;

    for (int y = 0; y < 25; ++y) {
        for (int x = 0; x < 80; ++x) {
            const int character = m_cpu->m_memory[0xb8000 + 2 * (x + y * 80)];
            const int attribute = m_cpu->m_memory[0xb8000 + 2 * (x + y * 80) + 1];
            
            // int coloridx = attribute >> 4 & 0b111;
            // auto gbcolor = COLORS[coloridx];
            // SDL_SetRenderDrawColor(render, gbcolor[0], gbcolor[1], gbcolor[2], SDL_ALPHA_OPAQUE);
            // SDL_Rect rect;
            // rect.x = x *7;
            // rect.y = y *12;
            // rect.w = 7;
            // rect.h = 12;
            // SDL_RenderDrawRect(render, &rect);




            // font
            // char chr = MAPPING[character];
            // if (chr != 0){
            //     printf("");
            // }
            // char chr = MAPPING[character];
            // SDL_Color White = {0, 200, 255};
            // SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, " ", White); 
            // SDL_Texture* Message = SDL_CreateTextureFromSurface(render, surfaceMessage);
            // SDL_Rect rect; //create a rect
            // rect.x = x *7;
            // rect.y = y *12;
            // rect.w = 7;
            // rect.h = 12;
            // SDL_RenderCopy(render, Message, NULL, &rect);
            // SDL_FreeSurface(surfaceMessage);
            // SDL_DestroyTexture(Message);

            // if (x + y * 80 == curLoc && (curAttr & 0b1) == 0b0 && clock() % 1000 < 500){

            // }else {
            
            // }
            //     g.drawString("_", x * 7, y * 12 + 9);
            // else
            //     g.drawString("" + mapping[character], x * 7, y * 12 + 9);
        }
    }

    // SDL_RenderClear(render);
    SDL_RenderPresent(render);

}
