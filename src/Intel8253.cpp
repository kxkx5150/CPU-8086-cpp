#include "Intel8253.h"

Intel8253::Intel8253(Intel8259 *pic) : pic(pic)
{
}
Intel8253::~Intel8253()
{
    delete pic;
}
bool Intel8253::isConnected(int port)
{
    return 0x40 <= port && port < 0x44;
}
void Intel8253::output(int sc, bool state)
{
    if (!output_status[sc] && state) {
        switch (sc) {
            case 0:    // TIMER 0
                pic->callIRQ(0);
                break;
        }
    }
    output_status[sc] = state;
}
int Intel8253::portIn(int w, int port)
{
    int sc = port & 0b11;
    switch (sc) {
        case 0b00:
        case 0b01:
        case 0b10: {
            int rl  = control[sc] >> 4 & 0b11;
            int val = count[sc];
            if (latched[sc]) {
                val = latch[sc];
                if (rl < 0b11 || !toggle[sc]) {
                    latched[sc] = false;
                }
            }
            switch (rl) {
                case 0b01:    // Read least significant byte only.
                    return val & 0xff;
                case 0b10:    // Read most significant byte only.
                    return val >> 8 & 0xff;
                case 0b11:    // Read lsb first, then msb.
                    if (!toggle[sc]) {
                        toggle[sc] = true;
                        return val & 0xff;
                    } else {
                        toggle[sc] = false;
                        return val >> 8 & 0xff;
                    }
            }
        }
    }
    return 0;
}
void Intel8253::portOut(int w, int port, int val)
{
    int sc = port & 0b11;
    switch (sc) {
        case 0b00:
        case 0b01:
        case 0b10: {
            int m  = control[sc] >> 1 & 0b111;
            int rl = control[sc] >> 4 & 0b11;

            switch (rl) {
                case 0b01:    // Load least significant byte only.
                    value[sc] = value[sc] & 0xff00 | val;
                    break;
                case 0b10:    // Load most significant byte only.
                    value[sc] = val << 8 | value[sc] & 0xff;
                    break;
                case 0b11:    // Load lsb first, then msb.
                    if (!toggle[sc]) {
                        toggle[sc] = true;
                        value[sc]  = value[sc] & 0xff00 | val;
                    } else {
                        toggle[sc] = false;
                        value[sc]  = val << 8 | value[sc] & 0xff;
                    }
                    break;
            }
            if (rl < 0b11 || !toggle[sc]) {
                count[sc]         = value[sc];
                enabled[sc]       = true;
                output_status[sc] = m == 0b10 || m == 0b11;
            }
            break;
        }
        case 0b11:
            sc = val >> 6 & 0b11;

            if ((val >> 4 & 0b11) == 0b00) {
                latch[sc]   = count[sc];
                latched[sc] = true;
            } else {
                control[sc] = val & 0xffff;
            }
            break;
    }
}
void Intel8253::tick()
{
    for (int sc = 0b00; sc < 0b11; ++sc) {
        if (enabled[sc]) {
            switch (control[sc] >> 1 & 0b111) {
                {
                    case 0b00:
                        count[sc] = --count[sc] & 0xffff;
                        if (count[sc] == 0) {
                            output(sc, true);
                        }
                        break;
                    case 0b10:
                        count[sc] = --count[sc] & 0xffff;

                        if (count[sc] == 1) {
                            count[sc] = value[sc];
                            output(sc, false);
                        } else {
                            output(sc, true);
                        }
                        break;
                    case 0b11:

                        if ((count[sc] & 0b1) == 0b1) {
                            if (output_status[sc]) {
                                count[sc] = count[sc] - 1 & 0xffff;
                            } else {
                                count[sc] = count[sc] - 3 & 0xffff;
                            }
                        } else {
                            count[sc] = count[sc] - 2 & 0xffff;
                        }

                        if (count[sc] == 0) {
                            count[sc] = value[sc];
                            output(sc, !output_status[sc]);
                        }
                        break;
                }
            }
        }
    }
}
