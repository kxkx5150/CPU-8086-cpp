#include "Motorola6845.h"

int Motorola6845::getRegister(int  index)
{
	return registers[index];
}

bool Motorola6845::isConnected(int  port)
{
	return port >= 0x3d0 && port < 0x3e0;
}

int Motorola6845::portIn(int  w, int  port)
{
	switch (port)
	{
	case 0x3da:
		// Simulate vertical/horizontal retracing.
		retrace = ++retrace % 4;
		switch (retrace)
		{
		case 0:
			return 8; // VR started
		case 1:
			return 0; // VR ended
		case 2:
			return 1; // HR started
		case 3:
			return 0; // HR ended
		}
	}
	return 0;
}

void Motorola6845::portOut(int  w, int  port, int  val)
{
	switch (port)
	{
	case 0x3d4: // Index
		index = val;
		break;
	case 0x3d5: // Register
		registers[index] = val;
		break;
	}
}
