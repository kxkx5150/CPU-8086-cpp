#pragma once
#include "Peripheral.h"
#include <vector>

class Intel8259 : public Peripheral {
  private:
    int              imr     = 0;
    int              irr     = 0;
    int              isr     = 0;
    int              icwStep = 0;
    std::vector<int> icw     = std::vector<int>(4);

  public:
    virtual void callIRQ(int line);
    virtual bool hasInt();
    virtual int  nextInt();

    bool isConnected(int port) override;
    int  portIn(int w, int port) override;
    void portOut(int w, int port, int val) override;
};
