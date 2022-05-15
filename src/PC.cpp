#include "PC.h"
#include "Intel8086.h"
#include "Intel8255.h"
#include "Motorola6845.h"

PC::PC()
{
    m_cpu = new Intel8086();
    m_cpu->init();
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
            
            int coloridx = attribute >> 4 & 0b111;
            auto gbcolor = COLORS[coloridx];
            SDL_SetRenderDrawColor(render, gbcolor[0], gbcolor[1], gbcolor[2], SDL_ALPHA_OPAQUE);

            SDL_Rect rect;
            rect.x = x *7;
            rect.y = y *12;
            rect.w = 7;
            rect.h = 12;
            SDL_RenderDrawRect(render, &rect);

            auto fgcolor = COLORS[attribute & 0b1111];

            // if (x + y * 80 == curLoc && (curAttr & 0b1) == 0b0 && System.currentTimeMillis() % 1000 < 500)
            //     g.drawString("_", x * 7, y * 12 + 9);
            // else
            //     g.drawString("" + mapping[character], x * 7, y * 12 + 9);
        }
    }
    SDL_RenderClear(render);
    SDL_RenderPresent(render);

}
