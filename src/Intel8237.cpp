#include "Intel8237.h"


bool Intel8237::isConnected(int  port)
{
	return 0x00 <= port && port < 0x20;
}
int Intel8237::portIn(int  w, int  port)
{
	int chan;
	switch (port)
	{
	case 0x00: // ADDR0
	case 0x02: // ADDR1
	case 0x04: // ADDR2
	case 0x06: // ADDR3
		chan = port / 2;
		if (!flipflop[chan])
		{
			flipflop[chan] = true;
			return addr[chan] & 0xff;
		}
		else
		{
			flipflop[chan] = false;
			return addr[chan] >> 8 & 0xff;
		}
	case 0x01: // CNT0
	case 0x03: // CNT1
	case 0x05: // CNT2
	case 0x07: // CNT3
		chan = (port - 1) / 2;
		if (!flipflop[chan])
		{
			flipflop[chan] = true;
			return cnt[chan] & 0xff;
		}
		else
		{
			flipflop[chan] = false;
			return cnt[chan] >> 8 & 0xff;
		}
	}
	return 0;
}
void Intel8237::portOut(int  w, int  port, int  val)
{
	int chan;
	switch (port)
	{
	case 0x00: // ADDR0
	case 0x02: // ADDR1
	case 0x04: // ADDR2
	case 0x06: // ADDR3
		chan = port / 2;
		if (!flipflop[chan])
		{
			flipflop[chan] = true;
			addr[chan] = addr[chan] & 0xff00 | val;
		}
		else
		{
			flipflop[chan] = false;
			addr[chan] = val << 8 | addr[chan] & 0xff;
		}
		break;
	case 0x01: // CNT0
	case 0x03: // CNT1
	case 0x05: // CNT2
	case 0x07: // CNT3
		chan = (port - 1) / 2;
		if (!flipflop[chan])
		{
			flipflop[chan] = true;
			cnt[chan] = cnt[chan] & 0xff00 | val;
		}
		else
		{
			flipflop[chan] = false;
			cnt[chan] = val << 8 | cnt[chan] & 0xff;
		}
		break;
	}
}

