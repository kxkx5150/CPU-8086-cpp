#pragma once
#include "Peripheral.h"
#include "Intel8259.h"
#include <vector>

class Intel8255 : public Peripheral
{
private:
	Intel8259* pic;
	std::vector<int> ports = std::vector<int>(4);

public:
	Intel8255(Intel8259*  pic);
	~Intel8255();

	virtual void keyTyped(int  scanCode);

	bool isConnected(int  port) override;
	int portIn(int  w, int  port) override;
	void portOut(int  w, int  port, int  val) override;
};
