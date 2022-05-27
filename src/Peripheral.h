#pragma once

class Peripheral {
  public:
    virtual bool isConnected(int port)             = 0;
    virtual int  portIn(int w, int port)           = 0;
    virtual void portOut(int w, int port, int val) = 0;
};
