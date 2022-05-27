#pragma once
#include "Peripheral.h"
#include <vector>

class Motorola6845 : public Peripheral {
  private:
    int              index     = 0;
    std::vector<int> registers = std::vector<int>(0x10);
    int              retrace   = 0;

  public:
    virtual int getRegister(int index);

    bool isConnected(int port) override;
    int  portIn(int w, int port) override;
    void portOut(int w, int port, int val) override;
};
