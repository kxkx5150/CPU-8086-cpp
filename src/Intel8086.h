#include <string>
//#include <vector>
#include "Intel8237.h"
#include "Intel8259.h"
#include "Intel8253.h"
#include "Intel8255.h"
#include "Motorola6845.h"

class Intel8237;
class Intel8259;
class Intel8253;
class Intel8255;
class Motorola6845;

class Intel8086 {
  public:
    uint8_t                   m_memory[0x100000]{};
    Intel8237                *m_dma  = nullptr;
    Intel8259                *m_pic  = nullptr;
    Intel8253                *m_pit  = nullptr;
    Intel8255                *m_ppi  = nullptr;
    Motorola6845             *m_crtc = nullptr;
    std::vector<Peripheral *> m_peripherals;

  private:
    int ah = 0, al = 0;
    int bh = 0, bl = 0;
    int ch = 0, cl = 0;
    int dh = 0, dl = 0;

    int sp    = 0;
    int bp    = 0;
    int si    = 0;
    int di    = 0;
    int cs    = 0;
    int ds    = 0;
    int ss    = 0;
    int es    = 0;
    int os    = 0;
    int ip    = 0;
    int flags = 0;

    std::vector<int> queue = std::vector<int>(6);

    int       op     = 0;
    int       d      = 0;
    int       w      = 0;
    int       mod    = 0;
    int       reg    = 0;
    int       rm     = 0;
    int       ea     = 0;
    long long clocks = 0;

    long long cycles = 0;

  public:
    Intel8086();
    ~Intel8086();

    void init();
    void reset();
    void load(int addr, std::string path);

    void run();
    void run_step(size_t steps, bool show_op);

  private:
    bool tick(bool show_op);
    bool cycle_opcode(int rep, bool show_op);
    bool exe_opcode(int rep, bool show_op);

    bool msb(int w, int x);
    int  shift(int x, int n);
    int  signconv(int w, int x);

    int adc(int w, int dst, int src);
    int add(int w, int dst, int src);
    int sbb(int w, int dst, int src);
    int sub(int w, int dst, int src);

    void callInt(int type);
    int  dec(int w, int dst);
    void decode();

    int getAddr(int seg, int off);
    int getEA(int mod, int rm);

    bool getFlag(int flag);
    int  getMem(int w);
    int  getMem(int w, int addr);
    int  getReg(int w, int reg);
    int  getRM(int w, int mod, int rm);
    int  getSegReg(int reg);

    void setFlag(int flag, bool set);
    void setFlags(int w, int res);
    void setMem(int w, int addr, int val);
    void setReg(int w, int reg, int val);
    void setRM(int w, int mod, int rm, int val);
    void setSegReg(int reg, int val);

    int  inc(int w, int dst);
    void logic(int w, int res);

    int  pop();
    void push(int val);
    int  portIn(int w, int port);
    void portOut(int w, int port, int val);

    void show_info(int op);
};
