#pragma once
#include "Peripheral.h"
#include "Intel8259.h"
#include <vector>

class Intel8253 : public Peripheral {
  private:
    Intel8259        *pic;
    std::vector<int>  count         = std::vector<int>(3);
    std::vector<int>  value         = std::vector<int>(3);
    std::vector<int>  latch         = std::vector<int>(3);
    std::vector<int>  control       = std::vector<int>(3);
    std::vector<bool> enabled       = std::vector<bool>(3);
    std::vector<bool> latched       = std::vector<bool>(3);
    std::vector<bool> output_status = std::vector<bool>(3);
    std::vector<bool> toggle        = std::vector<bool>(3);

  private:
    void output(int sc, bool state);

  public:
    Intel8253(Intel8259 *pic);
    ~Intel8253();

    bool isConnected(int port) override;
    int  portIn(int w, int port) override;
    void portOut(int w, int port, int val) override;

    virtual void tick();
};
