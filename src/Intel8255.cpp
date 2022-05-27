#include "Intel8255.h"

Intel8255::Intel8255(Intel8259 *pic) : pic(pic)
{
    ports[0] = 0x2c;
}
Intel8255::~Intel8255()
{
    delete pic;
}
void Intel8255::keyTyped(int scanCode)
{
    ports[0] = scanCode;
    pic->callIRQ(1);
}
bool Intel8255::isConnected(int port)
{
    return port >= 0x60 && port < 0x64;
}
int Intel8255::portIn(int w, int port)
{
    return ports[port & 0b11];
}
void Intel8255::portOut(int w, int port, int val)
{
    ports[port & 0b11] = val;
}
