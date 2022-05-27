#pragma once
#include "Peripheral.h"
#include <vector>

class Intel8237 : public Peripheral {
  private:
    std::vector<int>  addr     = std::vector<int>(4);
    std::vector<int>  cnt      = std::vector<int>(4);
    std::vector<bool> flipflop = std::vector<bool>(4);

  public:
    bool isConnected(int port) override;
    int  portIn(int w, int port) override;
    void portOut(int w, int port, int val) override;
};
