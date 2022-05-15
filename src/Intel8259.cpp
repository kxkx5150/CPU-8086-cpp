#include "Intel8259.h"

void Intel8259::callIRQ(int line)
{
    irr |= 1 << line;
}
bool Intel8259::hasInt()
{
    return (irr & ~imr) > 0;
}
bool Intel8259::isConnected(int port)
{
    return port == 0x20 || port == 0x21;
}
int Intel8259::nextInt()
{
    int bits = irr & ~imr;
    for (int i = 0; i < 8; ++i) {
        if ((bits >> i & 0b1) > 0) {
            {
                irr ^= 1 << i;
                isr |= 1 << i;
                return icw[1] + i;
            }
        }
    }
    return 0;
}
int Intel8259::portIn(int w, int port)
{
    switch (port) {
        case 0x20:
            return irr;
        case 0x21:
            return imr;
    }
    return 0;
}

void Intel8259::portOut(int w, int port, int val)
{
    switch (port) {
        case 0x20:
            if ((val & 0x10) > 0) {
                imr            = 0;
                icw[icwStep++] = val;
            }
            if ((val & 0x20) > 0)    // EOI
            {
                for (int i = 0; i < 8; ++i) {
                    if ((isr >> i & 0b1) > 0) {
                        isr ^= 1 << i;
                    }
                }
            }
            break;
        case 0x21:
            if (icwStep == 1) {
                icw[icwStep++] = val;
                if ((icw[0] & 0x02) > 0) {
                    ++icwStep;
                }
            } else if (icwStep < 4) {
                icw[icwStep++] = val;
            } else {
                imr = val;
            }
            break;
    }
}
