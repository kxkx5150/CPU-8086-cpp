#include "Intel8086.h"
#include <chrono>
#include <corecrt.h>
#include <crtdbg.h>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>

using namespace std;

Intel8086::Intel8086()
{
    m_dma         = new Intel8237();
    m_pic         = new Intel8259();
    m_pit         = new Intel8253(m_pic);
    m_ppi         = new Intel8255(m_pic);
    m_crtc        = new Motorola6845();
    m_peripherals = std::vector<Peripheral *>{m_dma, m_pic, m_pit, m_ppi, m_crtc};
}
Intel8086::~Intel8086()
{
    delete m_dma;
    delete m_pic;
    delete m_pit;
    delete m_ppi;
    delete m_crtc;
}
void Intel8086::init()
{
    reset();
    load(0xfe000, "bin\\bios.bin");
    load(0xf6000, "bin\\basic.bin");
}
void Intel8086::reset()
{
    flags = 0;
    ip    = 0x0000;
    cs    = 0xffff;
    ds    = 0x0000;
    ss    = 0x0000;
    es    = 0x0000;
    for (int i = 0; i < 6; i++) {
        queue[i] = 0;
    }
    clocks = 0;
}
void Intel8086::load(int addr, std::string path)
{
    FILE *f = fopen(path.c_str(), "rb");
    fseek(f, 0, SEEK_END);
    const int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    auto buffer = new uint8_t[size];
    fread(buffer, size, 1, f);
    for (int i = 0; i < size; i++) {
        m_memory[addr + i] = buffer[i] & 0xff;
    }
    fclose(f);
    delete[] buffer;
}
void Intel8086::run()
{
    tick(false);
}
void Intel8086::run_step(size_t steps, bool show_op)
{
    size_t i = 0;
    while (steps == 0 || steps != i) {
        i++;
        tick(show_op);
    }
    printf("");
}
bool Intel8086::tick(bool show_op)
{
    if (getFlag(TF)) {
        callInt(1);
        clocks += 50;
    }

    if (getFlag(IF) && m_pic->hasInt()) {
        callInt(m_pic->nextInt());
        clocks += 61;
    }

    os        = ds;
    int  rep  = 0;
    bool loop = true;
    while (loop) {
        // Segment prefix check.
        switch (getMem(B)) {
            case 0x26:    // ES
                os = es;
                clocks += 2;
                break;
            case 0x2e:    // CS
                os = cs;
                clocks += 2;
                break;
            case 0x36:    // SS
                os = ss;
                clocks += 2;
                break;
            case 0x3e:    // DS
                os = ds;
                clocks += 2;
                break;
            case 0xf2:    // repne/repnz
                rep = 2;
                clocks += 9;
                break;
            case 0xf3:    // rep/repe/repz
                rep = 1;
                clocks += 9;
                break;
            default:
                ip   = ip - 1 & 0xffff;
                loop = false;
        }
    }

    for (int i = 0; i < 6; ++i) {
        queue[i] = getMem(B, getAddr(cs, ip + i));
    }

    op = queue[0];
    d  = op >> 1 & 0b1;
    w  = op & 0b1;
    ip = ip + 1 & 0xffff;

    switch (op) {
        case 0xa4:    // movs
        case 0xa5:
        case 0xaa:    // stos
        case 0xab:
            if (rep == 0)
                ++clocks;
            break;
        case 0xa6:    // cmps
        case 0xa7:
        case 0xae:    // scas
        case 0xaf:
            break;
        case 0xac:    // lods
        case 0xad:
            if (rep == 0)
                --clocks;
            break;
        default:
            rep = 0;
            break;
    }
    return exe_opcode(rep, show_op);
}
bool Intel8086::exe_opcode(int rep, bool show_op)
{
    do {
        if (rep > 0) {
            int cx = getReg(W, CX);
            if (cx == 0) 
                break;
            setReg(W, CX, cx - 1);
        }

        while (clocks > 3) {
            clocks -= 4;
            m_pit->tick();
        }

        ea = -1;
        int dst, src, res;
        if (show_op)
            show_info(op);

        switch (op) {
            case 0x88:    // mov reg8/mem8,reg8
            case 0x89:    // mov reg16/mem16,reg16
            case 0x8a:    // mov reg8,reg8/mem8
            case 0x8b:    // mov reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    src = getReg(w, reg);
                    setRM(w, mod, rm, src);
                    clocks += mod == 0b11 ? 2 : 9;
                } else {
                    src = getRM(w, mod, rm);
                    setReg(w, reg, src);
                    clocks += mod == 0b11 ? 2 : 8;
                }
                break;
                // Immediate to Register/m_memory
            case 0xc6:    // mov reg8/mem8,immed8
            case 0xc7:    // mov reg16/mem16,immed16
                decode();
                switch (reg) {
                    case 0b000:
                        src = getMem(w);
                        setRM(w, mod, rm, src);
                }
                clocks += mod == 0b11 ? 4 : 10;
                break;
                // Immediate to Register
            case 0xb0:    // mov al,immed8
            case 0xb1:    // mov cl,immed8
            case 0xb2:    // mov dl,immed8
            case 0xb3:    // mov bl,immed8
            case 0xb4:    // mov ah,immed8
            case 0xb5:    // mov ch,immed8
            case 0xb6:    // mov dh,immed8
            case 0xb7:    // mov bh,immed8
            case 0xb8:    // mov ax,immed16
            case 0xb9:    // mov cx,immed16
            case 0xba:    // mov dx,immed16
            case 0xbb:    // mov bx,immed16
            case 0xbc:    // mov sp,immed16
            case 0xbd:    // mov bp,immed16
            case 0xbe:    // mov si,immed16
            case 0xbf:    // mov di,immed16
                w = op >> 3 & 0b1;
                reg = op & 0b111;
                src = getMem(w);
                setReg(w, reg, src);
                clocks += 4;
                break;
                // m_memory to/from Accumulator
            case 0xa0:    // mov al,mem8
            case 0xa1:    // mov ax,mem16
            case 0xa2:    // mov mem8,al
            case 0xa3:    // mov mem16,ax
                dst = getMem(W);
                if (d == 0b0) {
                    src = getMem(w, getAddr(os, dst));
                    setReg(w, AX, src);
                } else {
                    src = getReg(w, AX);
                    setMem(w, getAddr(os, dst), src);
                }
                clocks += 10;
                break;
                // Register/m_memory to/from Segment Register
            case 0x8c:    // mov reg16/mem16,segreg
            case 0x8e:    // mov segreg,reg16/mem16
                decode();
                if (d == 0b0) {
                    src = getSegReg(reg);
                    setRM(W, mod, rm, src);
                    clocks += mod == 0b11 ? 2 : 9;
                } else {
                    src = getRM(W, mod, rm);
                    setSegReg(reg, src);
                    clocks += mod == 0b11 ? 2 : 8;
                }
                break;
                // Register
            case 0x50:    // push ax
            case 0x51:    // push cx
            case 0x52:    // push dx
            case 0x53:    // push bx
            case 0x54:    // push sp
            case 0x55:    // push bp
            case 0x56:    // push si
            case 0x57:    // push di
                reg = op & 0b111;
                src = getReg(W, reg);
                push(src);
                clocks += 11;
                break;
                // Segment Register
            case 0x06:    // push es
            case 0x0e:    // push cs
            case 0x16:    // push ss
            case 0x1e:    // push ds
                    reg = op >> 3 & 0b111;
                src = getSegReg(reg);
                push(src);
                clocks += 10;
                break;
                // Register
            case 0x58:    // pop ax
            case 0x59:    // pop cx
            case 0x5a:    // pop dx
            case 0x5b:    // pop bx
            case 0x5c:    // pop sp
            case 0x5d:    // pop bp
            case 0x5e:    // pop si
            case 0x5f:    // pop di
                reg = op & 0b111;
                src = pop();
                setReg(W, reg, src);
                clocks += 8;
                break;
                // Segment Register
            case 0x07:    // pop es
            case 0x0f:    // pop cs
            case 0x17:    // pop ss
            case 0x1f:    // pop ds
                reg = op >> 3 & 0b111;
                src = pop();
                setSegReg(reg, src);
                clocks += 8;
                break;
                // Register/m_memory with Register
            case 0x86:    // xchg reg8,reg8/mem8
            case 0x87:    // xchg reg16,reg16/mem16
                decode();
                dst = getReg(w, reg);
                src = getRM(w, mod, rm);
                setReg(w, reg, src);
                setRM(w, mod, rm, dst);
                clocks += mod == 0b11 ? 3 : 17;
                break;
                // Register with Accumulator
            case 0x91:    // xchg ax,cx
            case 0x92:    // xchg ax,dx
            case 0x93:    // xchg ax,bx
            case 0x94:    // xchg ax,sp
            case 0x95:    // xchg ax,bp
            case 0x96:    // xchg ax,si
            case 0x97:    // xchg ax,di
                reg = op & 0b111;
                dst = getReg(W, AX);
                src = getReg(W, reg);
                setReg(W, AX, src);
                setReg(W, reg, dst);
                clocks += 3;
                break;
            case 0xd7:    // xlat source-table
                al = getMem(B, getAddr(os, getReg(W, BX) + al));
                clocks += 11;
                break;
                // Variable Port
            case 0xe4:    // in al,immed8
            case 0xe5:    // in ax,immed8
                src = getMem(B);
                setReg(w, AX, portIn(w, src));
                clocks += 10;
                if (w == W && (src & 0b1) == 0b1) {
                    clocks += 4;
                }
                break;
                // Fixed Port
            case 0xec:    // in al,dx
            case 0xed:    // in ax,dx
                src = getReg(W, DX);
                setReg(w, AX, portIn(w, src));
                clocks += 8;
                if (w == W && (src & 0b1) == 0b1) {
                    clocks += 4;
                }
                break;
                // Variable Port
            case 0xe6:    // out al,immed8
            case 0xe7:    // out ax,immed8
                src = getMem(B);
                portOut(w, src, getReg(w, AX));
                clocks += 10;
                if (w == W && (src & 0b1) == 0b1) {
                    clocks += 4;
                }
                break;
                // Fixed Port
            case 0xee:    // out al,dx
            case 0xef:    // out ax,dx
                src = getReg(W, DX);
                portOut(w, src, getReg(w, AX));
                clocks += 8;
                if (w == W && (src & 0b1) == 0b1) {
                    clocks += 4;
                }
                break;
            case 0x8d:    // lea reg16,mem16
                decode();
                src = getEA(mod, rm) - (os << 4);
                setReg(w, reg, src);
                clocks += 2;
                break;
            case 0xc5:    // lds reg16,mem32
                decode();
                src = getEA(mod, rm);
                setReg(w, reg, getMem(W, src));
                ds = getMem(W, src + 2);
                clocks += 16;
                break;
            case 0xc4:    // les reg16,mem32
                decode();
                src = getEA(mod, rm);
                setReg(w, reg, getMem(W, src));
                es = getMem(W, src + 2);
                clocks += 16;
                break;
            case 0x9f:    // lahf
                ah = flags & 0xff;
                clocks += 4;
                break;
            case 0x9e:    // sahf
                flags = flags & 0xff00 | ah;
                clocks += 4;
                break;
            case 0x9c:    // pushf
                push(flags);
                clocks += 10;
                break;
            case 0x9d:    // popf
                flags = pop();
                clocks += 8;
                break;
                // Reg./m_memory and Register to Either
            case 0x00:    // add reg8/mem8,reg8
            case 0x01:    // add reg16/mem16,reg16
            case 0x02:    // add reg8,reg8/mem8
            case 0x03:    // add reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = add(w, dst, src);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate to Accumulator
            case 0x04:    // add al,immed8
            case 0x05:    // add ax,immed16
                dst = getReg(w, 0);
                src = getMem(w);
                res = add(w, dst, src);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Reg./m_memory with Register to Either
            case 0x10:    // adc reg8/mem8,reg8
            case 0x11:    // adc reg16/mem16,reg16
            case 0x12:    // adc reg8,reg8/mem8
            case 0x13:    // adc reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = adc(w, dst, src);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate to Accumulator
            case 0x14:    // adc al,immed8
            case 0x15:    // adc ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                res = adc(w, dst, src);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Register
            case 0x40:    // inc ax
            case 0x41:    // inc cx
            case 0x42:    // inc dx
            case 0x43:    // inc bx
            case 0x44:    // inc sp
            case 0x45:    // inc bp
            case 0x46:    // inc si
            case 0x47:    // inc di
                reg = op & 0b111;
                src = getReg(W, reg);
                res = inc(W, src);
                setReg(W, reg, res);
                clocks += 2;
                break;
            case 0x37:    // aaa
                if ((al & 0xf) > 9 || getFlag(AF)) {
                    al += 6;
                    ah = ah + 1 & 0xff;
                    setFlag(CF, true);
                    setFlag(AF, true);
                } else {
                    setFlag(CF, false);
                    setFlag(AF, false);
                }
                al &= 0xf;
                clocks += 4;
                break;
            case 0x27: {    // daa
                int  oldAL = al;
                bool oldCF = getFlag(CF);
                setFlag(CF, false);
                if ((al & 0xf) > 9 || getFlag(AF)) {
                    al += 6;
                    setFlag(CF, oldCF || al < 0);
                    al &= 0xff;
                    setFlag(AF, true);
                } else {
                    setFlag(AF, false);
                }
                if (oldAL > 0x99 || oldCF) {
                    al = al + 0x60 & 0xff;
                    setFlag(CF, true);
                } else {
                    setFlag(CF, false);
                }
                setFlags(B, al);
                clocks += 4;
                break;
            }
                // Reg./m_memory and Register to Either
            case 0x28:    // sub reg8/mem8,reg8
            case 0x29:    // sub reg16/mem16,reg16
            case 0x2a:    // sub reg8,reg8/mem8
            case 0x2b:    // sub reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = sub(w, dst, src);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate from Accumulator
            case 0x2c:    // sub al,immed8
            case 0x2d:    // sub ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                res = sub(w, dst, src);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Reg./m_memory with Register to Either
            case 0x18:    // sbb reg8/mem8,reg8
            case 0x19:    // sbb reg16/mem16,reg16
            case 0x1a:    // sbb reg8,reg8/mem8
            case 0x1b:    // sbb reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = sbb(w, dst, src);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate to Accumulator
            case 0x1c:    // sbb al,immed8
            case 0x1d:    // sbb ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                res = sbb(w, dst, src);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Register
            case 0x48:    // dec ax
            case 0x49:    // dec cx
            case 0x4a:    // dec dx
            case 0x4b:    // dec bx
            case 0x4c:    // dec sp
            case 0x4d:    // dec bp
            case 0x4e:    // dec si
            case 0x4f:    // dec di
                reg = op & 0b111;
                dst = getReg(W, reg);
                res = dec(W, dst);
                setReg(W, reg, res);
                clocks += 2;
                break;
                // Register/m_memory and Register
            case 0x38:    // cmp reg8/mem8,reg8
            case 0x39:    // cmp reg16/mem16,reg16
            case 0x3a:    // cmp reg8,reg8/mem8
            case 0x3b:    // cmp reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                sub(w, dst, src);
                clocks += mod == 0b11 ? 3 : 9;
                break;
                // Immediate with Accumulator
            case 0x3c:    // cmp al,immed8
            case 0x3d:    // cmp ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                sub(w, dst, src);
                clocks += 4;
                break;
            case 0x3f:    // AAS
                if ((al & 0xf) > 9 || getFlag(AF)) {
                    al -= 6;
                    ah = ah - 1 & 0xff;
                    setFlag(CF, true);
                    setFlag(AF, true);
                } else {
                    setFlag(CF, false);
                    setFlag(AF, false);
                }
                al &= 0xf;
                clocks += 4;
                break;
            case 0x2f:    // DAS
            {
                int  oldAL = al;
                bool oldCF = getFlag(CF);
                setFlag(CF, false);
                if ((al & 0xf) > 9 || getFlag(AF)) {
                    al -= 6;
                    setFlag(CF, oldCF || (al & 0xff) > 0);
                    al &= 0xff;
                    setFlag(AF, true);
                } else {
                    setFlag(AF, false);
                }
                if (oldAL > 0x99 || oldCF) {
                    al = al - 0x60 & 0xff;
                    setFlag(CF, true);
                } else {
                    setFlag(CF, false);
                }
                setFlags(B, al);
                clocks += 4;
                break;
            }
            case 0xd4:    // AAM
                src = getMem(B);
                if (src == 0) {
                    callInt(0);
                } else {
                    ah = al / src & 0xff;
                    al = al % src & 0xff;
                    setFlags(W, getReg(W, AX));
                    clocks += 83;
                }
                break;
            case 0xd5:    // AAD
                src = getMem(B);
                al  = ah * src + al & 0xff;
                ah  = 0;
                setFlags(B, al);
                clocks += 60;
                break;
            case 0x98:    // CBW
                if ((al & 0x80) == 0x80) {
                    ah = 0xff;
                } else {
                    ah = 0x00;
                }
                clocks += 2;
                break;
            case 0x99:    // CWD
                if ((ah & 0x80) == 0x80) {
                    setReg(W, DX, 0xffff);
                } else {
                    setReg(W, DX, 0x0000);
                }
                clocks += 5;
                break;
                // Register/m_memory and Register
                // case 0x20:    // and reg8/mem8,reg8
                // case 0x21:    // and reg16/mem16,reg16
                // case 0x22:    // and reg8,reg8/mem8
                // case 0x23:    // and reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = dst & src;
                logic(w, res);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate to Accumulator
            case 0x24:    // and al,immed8
            case 0x25:    // and ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                res = dst & src;
                logic(w, res);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Register/m_memory and Register
            case 0x08:    // or reg8/mem8,reg8
            case 0x09:    // or reg16/mem16,reg16
            case 0x0a:    // or reg8,reg8/mem8
            case 0x0b:    // or reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = dst | src;
                logic(w, res);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate to Accumulator
            case 0x0c:    // or al,immed8
            case 0x0d:    // or ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                res = dst | src;
                logic(w, res);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Register/m_memory and Register
            case 0x30:    // xor reg8/mem8,reg8
            case 0x31:    // xor reg16/mem16,reg16
            case 0x32:    // xor reg8,reg8/mem8
            case 0x33:    // xor reg16,reg16/mem16
                decode();
                if (d == 0b0) {
                    dst = getRM(w, mod, rm);
                    src = getReg(w, reg);
                } else {
                    dst = getReg(w, reg);
                    src = getRM(w, mod, rm);
                }
                res = dst ^ src;
                logic(w, res);
                if (d == 0b0) {
                    setRM(w, mod, rm, res);
                    clocks += mod == 0b11 ? 3 : 16;
                } else {
                    setReg(w, reg, res);
                    clocks += mod == 0b11 ? 3 : 9;
                }
                break;
                // Immediate to Accumulator
            case 0x34:    // xor al,immed8
            case 0x35:    // xor ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                res = dst ^ src;
                logic(w, res);
                setReg(w, AX, res);
                clocks += 4;
                break;
                // Register/m_memory and Register
            case 0x84:    // test reg8/mem8,reg8
            case 0x85:    // test reg16/mem16,reg16
                decode();
                dst = getRM(w, mod, rm);
                src = getReg(w, reg);
                logic(w, dst & src);
                clocks += mod == 0b11 ? 3 : 9;
                break;
                // Immediate and Accumulator
            case 0xa8:    // test al,immed8
            case 0xa9:    // test ax,immed16
                dst = getReg(w, AX);
                src = getMem(w);
                logic(w, dst & src);
                clocks += 4;
                break;
            case 0xa4:    // movs dest-str8,src-str8
            case 0xa5:    // movs dest-str16,src-str16
                src = getMem(w, getAddr(os, si));
                setMem(w, getAddr(es, di), src);
                si = si + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                di = di + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                clocks += 17;
                break;
            case 0xa6:    // cmps dest-str8,src-str8
            case 0xa7:    // cmps dest-str16,src-str16
                dst = getMem(w, getAddr(es, di));
                src = getMem(w, getAddr(os, si));
                sub(w, src, dst);
                si = si + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                di = di + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                if (rep == 1 && !getFlag(ZF) || rep == 2 && getFlag(ZF)) {
                    rep = 0;
                }
                clocks += 22;
                break;
            case 0xae:    // scas dest-str8
            case 0xaf:    // scas dest-str16
                dst = getMem(w, getAddr(es, di));
                src = getReg(w, AX);
                sub(w, src, dst);
                di = di + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                if (rep == 1 && !getFlag(ZF) || rep == 2 && getFlag(ZF)) {
                    rep = 0;
                }
                clocks += 15;
                break;
            case 0xac:    // lods src-str8
            case 0xad:    // lods src-str16
                src = getMem(w, getAddr(os, si));
                setReg(w, AX, src);
                si = si + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                clocks += 13;
                break;
            case 0xaa:    // stos dest-str8
            case 0xab:    // stos dest-str16
                src = getReg(w, AX);
                setMem(w, getAddr(es, di), src);
                di = di + (getFlag(DF) ? -1 : 1) * (1 + w) & 0xffff;
                clocks += 10;
                break;
                // Direct with Segment
            case 0xe8:    // call near-proc
                dst = getMem(W);
                dst = signconv(W, dst);
                push(ip);
                ip = ip + dst & 0xffff;
                clocks += 19;
                break;
                // Direct Intersegment
            case 0x9a:    // call far-proc
                dst = getMem(W);
                src = getMem(W);
                push(cs);
                push(ip);
                ip = dst;
                cs = src;
                clocks += 28;
                break;
                // Within Segment
            case 0xc3:    // RET (intrasegment)
                ip = pop();
                clocks += 8;
                break;
                // Within Seg Adding Immed to SP
            case 0xc2:    // ret immed16 (intraseg)
                src = getMem(W);
                ip  = pop();
                sp += src;
                clocks += 12;
                break;
                // Intersegment
            case 0xcb:    // RET (intersegment)
                ip = pop();
                cs = pop();
                clocks += 18;
                break;
                // Intersegment Adding Immediate to SP
            case 0xca:    // ret immed16 (intersegment)
                src = getMem(W);
                ip  = pop();
                cs  = pop();
                sp += src;
                clocks += 17;
                break;
                // Direct within Segment
            case 0xe9:    // jmp near-label
                dst = getMem(W);
                dst = signconv(W, dst);
                ip  = ip + dst & 0xffff;
                clocks += 15;
                break;
                // Direct within Segment-Short
            case 0xeb:    // jmp short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                ip  = ip + dst & 0xffff;
                clocks += 15;
                break;
                // Direct Intersegment
            case 0xea:    // jmp far-label
                dst = getMem(W);
                src = getMem(W);
                ip  = dst;
                cs  = src;
                clocks += 15;
                break;
            case 0x70:    // jo short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(OF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x71:    // jno short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!getFlag(OF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x72:    // jb/jnae/jc short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(CF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x73:    // jnb/jae/jnc short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!getFlag(CF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x74:    // je/jz short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(ZF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x75:    // jne/jnz short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!getFlag(ZF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x76:    // jbe/jna short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(CF) | getFlag(ZF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x77:    // jnbe/ja short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!(getFlag(CF) | getFlag(ZF))) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x78:    // js short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(SF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x79:    // jns short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!getFlag(SF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x7a:    // jp/jpe short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(PF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x7b:    // jnp/jpo short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!getFlag(PF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x7c:    // jl/jnge short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(SF) ^ getFlag(OF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x7d:    // jnl/jge short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!(getFlag(SF) ^ getFlag(OF))) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x7e:    // jle/jng short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getFlag(SF) ^ getFlag(OF) | getFlag(ZF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0x7f:    // jnle/jg short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (!(getFlag(SF) ^ getFlag(OF) | getFlag(ZF))) {
                    ip = ip + dst & 0xffff;
                    clocks += 16;
                } else {
                    clocks += 4;
                }
                break;
            case 0xe2:    // loop short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                src = getReg(W, CX) - 1 & 0xffff;
                setReg(W, CX, src);
                if (src != 0) {
                    ip = ip + dst & 0xffff;
                    clocks += 17;
                } else {
                    clocks += 5;
                }
                break;
            case 0xe1:    // loope/loopz short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                src = getReg(W, CX) - 1 & 0xffff;
                setReg(W, CX, src);
                if (src != 0 && getFlag(ZF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 18;
                } else {
                    clocks += 6;
                }
                break;
            case 0xe0:    // loopne/loopnz short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                src = getReg(W, CX) - 1 & 0xffff;
                setReg(W, CX, src);
                if (src != 0 && !getFlag(ZF)) {
                    ip = ip + dst & 0xffff;
                    clocks += 19;
                } else {
                    clocks += 5;
                }
                break;
            case 0xe3:    // jcxz short-label
                dst = getMem(B);
                dst = signconv(B, dst);
                if (getReg(W, CX) == 0) {
                    ip = ip + dst & 0xffff;
                    clocks += 18;
                } else {
                    clocks += 6;
                }
                break;
                // Type 3
            case 0xcc:    // INT 3
                          // Type Specified
            case 0xcd:    // int immed8
                callInt(op == 0xcc ? 3 : getMem(B));
                clocks += op == 0xcc ? 52 : 51;
                break;
            case 0xce:    // INTO
                if (getFlag(OF)) {
                    callInt(4);
                    clocks += 53;
                } else {
                    clocks += 4;
                }
                break;
            case 0xcf:    // IRET
                ip    = pop();
                cs    = pop();
                flags = pop();
                clocks += 24;
                break;
            case 0xf8:    // CLC
                setFlag(CF, false);
                clocks += 2;
                break;
            case 0xf5:    // CMC
                setFlag(CF, !getFlag(CF));
                clocks += 2;
                break;
            case 0xf9:    // STC
                setFlag(CF, true);
                clocks += 2;
                break;
            case 0xfc:    // CLD
                setFlag(DF, false);
                clocks += 2;
                break;
            case 0xfd:    // STD
                setFlag(DF, true);
                clocks += 2;
                break;
            case 0xfa:    // CLI
                setFlag(IF, false);
                clocks += 2;
                break;
            case 0xfb:    // STI
                setFlag(IF, true);
                clocks += 2;
                break;
            case 0xf4:    // HLT
                clocks += 2;
                return false;
            case 0x9b:    // WAIT
                clocks += 3;
                break;
            case 0xd8:    // esc 0,source
            case 0xd9:    // esc 1,source
            case 0xda:    // esc 2,source
            case 0xdb:    // esc 3,source
            case 0xdc:    // esc 4,source
            case 0xdd:    // esc 5,source
            case 0xde:    // esc 6,source
            case 0xdf:    // esc 7,source
                decode();
                clocks += mod == 0b11 ? 2 : 8;
                break;
            case 0xf0:    // LOCK
                clocks += 2;
                break;
            case 0x90:    // NOP
                clocks += 3;
                break;
            case 0x80:
                // add reg8/mem8,immed8
                // or reg8/mem8,immed8
                // adc reg8/mem8,immed8
                // sbb reg8/mem8,immed8
                // and reg8/mem8,immed8
                // sub reg8/mem8,immed8
                // xor reg8/mem8,immed8
                // cmp reg8/mem8,immed8
            case 0x81:
                // add reg16/mem16,immed16
                // or reg16/mem16,immed16
                // adc reg16/mem16,immed16
                // sbb reg16/mem16,immed16
                // and reg16/mem16,immed16
                // sub reg16/mem16,immed16
                // xor reg16/mem16,immed16
                // cmp reg16/mem16,immed16
            case 0x82:
                // add reg8/mem8,immed8
                // adc reg8/mem8,immed8
                // sbb reg8/mem8,immed8
                // sub reg8/mem8,immed8
                // cmp reg8/mem8,immed8
            case 0x83:
                // add reg16/mem16,immed8
                // adc reg16/mem16,immed8
                // sbb reg16/mem16,immed8
                // sub reg16/mem16,immed8
                // cmp reg16/mem16,immed8
                decode();
                dst = getRM(w, mod, rm);
                src = getMem(B);
                if (op == 0x81) {
                    src |= getMem(B) << 8;
                }
                // Perform sign extension if needed.
                else if (op == 0x83 && (src & SIGN[B]) > 0) {
                    src |= 0xff00;
                }
                switch (reg) {
                    case 0b000:    // ADD
                        res = add(w, dst, src);
                        setRM(w, mod, rm, res);
                        break;
                    case 0b001:    // OR
                        if (op == 0x80 || op == 0x81) {
                            res = dst | src;
                            logic(w, res);
                            setRM(w, mod, rm, res);
                            break;
                        }
                        break;
                    case 0b010:    // ADC
                        res = adc(w, dst, src);
                        setRM(w, mod, rm, res);
                        break;
                    case 0b011:    // SBB
                        res = sbb(w, dst, src);
                        setRM(w, mod, rm, res);
                        break;
                    case 0b100:    // AND
                        if (op == 0x80 || op == 0x81) {
                            res = dst & src;
                            logic(w, res);
                            setRM(w, mod, rm, res);
                        }
                        break;
                    case 0b101:    // SUB
                        res = sub(w, dst, src);
                        setRM(w, mod, rm, res);
                        break;
                    case 0b110:    // XOR
                        if (op == 0x80 || op == 0x81) {
                            res = dst ^ src;
                            logic(w, res);
                            setRM(w, mod, rm, res);
                        }
                        break;
                    case 0b111:    // CMP
                        sub(w, dst, src);
                        if (mod == 0b11) {
                            clocks -= 7;
                        }
                        break;
                }
                clocks += mod == 0b11 ? 4 : 17;
                break;
            case 0x8f:
                // POP REG16/MEM16
                decode();
                switch (reg) {
                    case 0b000:    // POP
                        src = pop();
                        setRM(w, mod, rm, src);
                        break;
                }
                clocks += mod == 0b11 ? 8 : 17;
                break;
            case 0xd0:
                // rol reg8/mem8,1
                // ror reg8/mem8,1
                // rcl reg8/mem8,1
                // rcr reg8/mem8,1
                // sal/shl reg8/mem8,1
                // shr reg8/mem8,1
                // sar reg8/mem8,1
            case 0xd1:
                // rol reg16/mem16,1
                // ror reg16/mem16,1
                // rcl reg16/mem16,1
                // rcr reg16/mem16,1
                // sal/shl reg16/mem16,1
                // shr reg16/mem16,1
                // sar reg16/mem16,1
            case 0xd2:
                // rol reg8/mem8,cl
                // ror reg8/mem8,cl
                // rcl reg8/mem8,cl
                // rcr reg8/mem8,cl
                // sal/shl reg8/mem8,cl
                // shr reg8/mem8,cl
                // sar reg8/mem8,cl
            case 0xd3: {
                // rol reg16/mem16,cl
                // ror reg16/mem16,cl
                // rcl reg16/mem16,cl
                // rcr reg16/mem16,cl
                // sal/shl reg16/mem16,cl
                // shr reg16/mem16,cl
                // sar reg16/mem16,cl

                decode();
                dst = getRM(w, mod, rm);
                src = op == 0xd0 || op == 0xd1 ? 1 : cl;
                bool tempCF;
                switch (reg) {
                    case 0b000:    // ROL
                        for (int cnt = 0; cnt < src; ++cnt) {
                            tempCF = msb(w, dst);
                            dst <<= 1;
                            dst |= tempCF ? 0b1 : 0b0;
                            dst &= MASK[w];
                        }
                        setFlag(CF, (dst & 0b1) == 0b1);
                        if (src == 1) {
                            setFlag(OF, msb(w, dst) ^ getFlag(CF));
                        }
                        break;
                    case 0b001:    // ROR
                        for (int cnt = 0; cnt < src; ++cnt) {
                            tempCF = (dst & 0b1) == 0b1;
                            dst    = static_cast<int>(static_cast<unsigned int>(dst) >> 1);
                            dst |= (tempCF ? 1 : 0) * SIGN[w];
                            dst &= MASK[w];
                        }
                        setFlag(CF, msb(w, dst));
                        if (src == 1) {
                            setFlag(OF, msb(w, dst) ^ msb(w, dst << 1));
                        }
                        break;
                    case 0b010:    // RCL
                        for (int cnt = 0; cnt < src; ++cnt) {
                            tempCF = msb(w, dst);
                            dst <<= 1;
                            dst |= getFlag(CF) ? 0b1 : 0b0;
                            dst &= MASK[w];
                            setFlag(CF, tempCF);
                        }
                        if (src == 1) {
                            setFlag(OF, msb(w, dst) ^ getFlag(CF));
                        }
                        break;
                    case 0b011:    // RCR
                        if (src == 1) {
                            setFlag(OF, msb(w, dst) ^ getFlag(CF));
                        }
                        for (int cnt = 0; cnt < src; ++cnt) {
                            tempCF = (dst & 0b1) == 0b1;
                            dst    = static_cast<int>(static_cast<unsigned int>(dst) >> 1);
                            dst |= (getFlag(CF) ? 1 : 0) * SIGN[w];
                            dst &= MASK[w];
                            setFlag(CF, tempCF);
                        }
                        break;
                    case 0b100:    // SAL/SHL
                        for (int cnt = 0; cnt < src; ++cnt) {
                            setFlag(CF, (dst & SIGN[w]) == SIGN[w]);
                            dst <<= 1;
                            dst &= MASK[w];
                        }
                        // Determine overflow.
                        if (src == 1) {
                            setFlag(OF, ((dst & SIGN[w]) == SIGN[w]) ^ getFlag(CF));
                        }
                        if (src > 0) {
                            setFlags(w, dst);
                        }
                        break;
                    case 0b101:    // SHR
                        // Determine overflow.
                        if (src == 1) {
                            setFlag(OF, (dst & SIGN[w]) == SIGN[w]);
                        }
                        for (int cnt = 0; cnt < src; ++cnt) {
                            setFlag(CF, (dst & 0b1) == 0b1);
                            dst = static_cast<int>(static_cast<unsigned int>(dst) >> 1);
                            dst &= MASK[w];
                        }
                        if (src > 0) {
                            setFlags(w, dst);
                        }
                        break;
                    case 0b111:    // SAR
                        // Determine overflow.
                        if (src == 1) {
                            setFlag(OF, false);
                        }
                        for (int cnt = 0; cnt < src; ++cnt) {
                            setFlag(CF, (dst & 0b1) == 0b1);
                            dst = signconv(w, dst);
                            dst >>= 1;
                            dst &= MASK[w];
                        }
                        if (src > 0) {
                            setFlags(w, dst);
                        }
                        break;
                }
                setRM(w, mod, rm, dst);
                if (op == 0xd0 || op == 0xd1) {
                    clocks += mod == 0b11 ? 2 : 15;
                } else {
                    clocks += mod == 0b11 ? 8 + 4 * src : 20 + 4 * src;
                }
                break;
            }
            case 0xf6:
                // test reg8/mem8,immed8
                // not reg8/mem8
                // neg reg8/mem8
                // mul reg8/mem8
                // imul reg8/mem8
                // div reg8/mem8
                // idiv reg8/mem8
            case 0xf7:
                // test reg16/mem16,immed16
                // not reg16/mem16
                // neg reg16/mem16
                // mul reg16/mem16
                // imul reg16/mem16
                // div reg16/mem16
                // idiv reg16/mem16
                decode();
                src = getRM(w, mod, rm);
                switch (reg) {
                    case 0b000:    // TEST
                        dst = getMem(w);
                        logic(w, dst & src);
                        clocks += mod == 0b11 ? 5 : 11;
                        break;
                    case 0b010:    // NOT
                        setRM(w, mod, rm, ~src);
                        clocks += mod == 0b11 ? 3 : 16;
                        break;
                    case 0b011:    // NEG
                        dst = sub(w, 0, src);
                        setFlag(CF, dst > 0);
                        setRM(w, mod, rm, dst);
                        clocks += mod == 0b11 ? 3 : 16;
                        break;
                    case 0b100:    // MUL
                        if (w == B) {
                            dst = al;
                            res = dst * src & 0xffff;
                            setReg(W, AX, res);
                            if (ah > 0) {
                                setFlag(CF, true);
                                setFlag(OF, true);
                            } else {
                                setFlag(CF, false);
                                setFlag(OF, false);
                            }
                            clocks += mod == 0b11 ? (77 - 70) / 2 : (83 - 76) / 2;
                        } else {
                            dst            = getReg(W, AX);
                            long long lres = static_cast<long long>(dst) * static_cast<long long>(src) & 0xffffffff;
                            setReg(W, AX, static_cast<int>(lres));
                            setReg(
                                W, DX,
                                static_cast<int>(static_cast<long long>(static_cast<unsigned long long>(lres) >> 16)));
                            if (getReg(W, DX) > 0) {
                                setFlag(CF, true);
                                setFlag(OF, true);
                            } else {
                                setFlag(CF, false);
                                setFlag(OF, false);
                            }
                            clocks += mod == 0b11 ? (133 - 118) / 2 : (139 - 124) / 2;
                        }
                        break;
                    case 0b101:    // IMUL
                        if (w == B) {
                            src = signconv(B, src);
                            dst = al;
                            dst = signconv(B, dst);
                            res = dst * src & 0xffff;
                            setReg(W, AX, res);
                            if (ah > 0x00 && ah < 0xff) {
                                setFlag(CF, true);
                                setFlag(OF, true);
                            } else {
                                setFlag(CF, false);
                                setFlag(OF, false);
                            }
                            clocks += mod == 0b11 ? (98 - 80) / 2 : (154 - 128) / 2;
                        } else {
                            src            = signconv(W, src);
                            dst            = ah << 8 | al;
                            dst            = signconv(W, dst);
                            long long lres = static_cast<long long>(dst) * static_cast<long long>(src) & 0xffffffff;
                            setReg(W, AX, static_cast<int>(lres));
                            setReg(
                                W, DX,
                                static_cast<int>(static_cast<long long>(static_cast<unsigned long long>(lres) >> 16)));
                            int dx = getReg(W, DX);
                            if (dx > 0x0000 && dx < 0xffff) {
                                setFlag(CF, true);
                                setFlag(OF, true);
                            } else {
                                setFlag(CF, false);
                                setFlag(OF, false);
                            }
                            clocks += mod == 0b11 ? (104 - 86) / 2 : (160 - 134) / 2;
                        }
                        break;
                    case 0b110:    // DIV
                        if (src == 0) {
                            callInt(0);
                        } else if (w == B) {
                            dst = ah << 8 | al;
                            res = dst / src & 0xffff;
                            if (res > 0xff) {
                                callInt(0);
                            } else {
                                al = res & 0xff;
                                ah = dst % src & 0xff;
                            }
                            clocks += mod == 0b11 ? (90 - 80) / 2 : (96 - 86) / 2;
                        } else {
                            long long ldst = static_cast<long long>(getReg(W, DX)) << 16 | getReg(W, AX);
                            long long lres = ldst / src & 0xffffffff;
                            if (lres > 0xffff) {
                                callInt(0);
                            } else {
                                setReg(W, AX, static_cast<int>(lres));
                                lres = ldst % src & 0xffff;
                                setReg(W, DX, static_cast<int>(lres));
                            }
                            clocks += mod == 0b11 ? (162 - 144) / 2 : (168 - 150) / 2;
                        }
                        break;
                    case 0b111:    // IDIV
                        if (src == 0) {
                            callInt(0);
                        } else if (w == B) {
                            src = signconv(B, src);
                            dst = getReg(W, AX);
                            dst = signconv(W, dst);
                            res = dst / src & 0xffff;
                            if (res > 0x007f && res < 0xff81) {
                                callInt(0);
                            } else {
                                al = res & 0xff;
                                ah = dst % src & 0xff;
                            }
                            clocks += mod == 0b11 ? (112 - 101) / 2 : (118 - 107) / 2;
                        } else {
                            src            = signconv(W, src);
                            long long ldst = static_cast<long long>(getReg(W, DX)) << 16 | getReg(W, AX);
                            // Do sign conversion manually.
                            ldst           = ldst << 32 >> 32;
                            long long lres = ldst / src & 0xffffffff;
                            if (lres > 0x00007fff || lres < 0xffff8000) {
                                callInt(0);
                            } else {
                                setReg(W, AX, static_cast<int>(lres));
                                lres = ldst % src & 0xffff;
                                setReg(W, DX, static_cast<int>(lres));
                            }
                            clocks += mod == 0b11 ? (184 - 165) / 2 : (190 - 171) / 2;
                        }
                        break;
                }
                break;
            case 0xfe:
                // inc reg8/mem8
                // dec reg8/mem8
                decode();
                src = getRM(w, mod, rm);
                switch (reg) {
                    case 0b000:    // INC
                        res = inc(w, src);
                        setRM(w, mod, rm, res);
                        break;
                    case 0b001:    // DEC
                        res = dec(w, src);
                        setRM(w, mod, rm, res);
                        break;
                }
                clocks += mod == 0b11 ? 3 : 15;
                break;
            case 0xff:
                // inc reg16/mem16
                // dec reg16/mem16
                // call reg16/mem16 (intra)
                // call mem16 (intersegment)
                // jmp reg16/mem16 (intra)
                // jmp mem16 (intersegment)
                // push reg16/mem16
                decode();
                src = getRM(w, mod, rm);
                switch (reg) {
                    case 0b000:    // INC
                        res = inc(w, src);
                        setRM(w, mod, rm, res);
                        clocks += mod == 0b11 ? 3 : 15;
                        break;
                    case 0b001:    // DEC
                        res = dec(w, src);
                        setRM(w, mod, rm, res);
                        clocks += mod == 0b11 ? 3 : 15;
                        break;
                    case 0b010:    // CALL
                        push(ip);
                        ip = src;
                        clocks += mod == 0b11 ? 16 : 21;
                        break;
                    case 0b011:    // CALL
                        push(cs);
                        push(ip);
                        dst = getEA(mod, rm);
                        ip  = getMem(W, dst);
                        cs  = getMem(W, dst + 2);
                        clocks += 37;
                        break;
                    case 0b100:    // JMP
                        ip = src;
                        clocks += mod == 0b11 ? 11 : 18;
                        break;
                    case 0b101:    // JMP
                        dst = getEA(mod, rm);
                        ip  = getMem(W, dst);
                        cs  = getMem(W, dst + 2);
                        clocks += 24;
                        break;
                    case 0b110:    // PUSH
                        push(src);
                        clocks += mod == 0b11 ? 11 : 16;
                        break;
                }
                break;
        }
    } while (rep > 0);
    return true;
}
bool Intel8086::msb(int w, int x)
{
    return (x & SIGN[w]) == SIGN[w];
}
int Intel8086::shift(int x, int n)
{
    return n >= 0 ? x << n : x >> -n;
}
int Intel8086::signconv(int w, int x)
{
    return x << (32 - BITS[w]) >> (32 - BITS[w]);
}
int Intel8086::adc(int w, int dst, int src)
{
    int carry = (flags & CF) == CF ? 1 : 0;
    int res   = dst + src + carry & MASK[w];
    setFlag(CF, carry == 1 ? res <= dst : res < dst);
    setFlag(AF, ((res ^ dst ^ src) & AF) > 0);
    setFlag(OF, (shift((dst ^ src ^ -1) & (dst ^ res), 12 - BITS[w]) & OF) > 0);
    setFlags(w, res);
    return res;
}
int Intel8086::add(int w, int dst, int src)
{
    int res = dst + src & MASK[w];
    setFlag(CF, res < dst);
    setFlag(AF, ((res ^ dst ^ src) & AF) > 0);
    setFlag(OF, (shift((dst ^ src ^ -1) & (dst ^ res), 12 - BITS[w]) & OF) > 0);
    setFlags(w, res);
    return res;
}
int Intel8086::sbb(int w, int dst, int src)
{
    int carry = (flags & CF) == CF ? 1 : 0;
    int res   = dst - src - carry & MASK[w];
    setFlag(CF, carry > 0 ? dst <= src : dst < src);
    setFlag(AF, ((res ^ dst ^ src) & AF) > 0);
    setFlag(OF, (shift((dst ^ src) & (dst ^ res), 12 - BITS[w]) & OF) > 0);
    setFlags(w, res);
    return res;
}
int Intel8086::sub(int w, int dst, int src)
{
    int res = dst - src & MASK[w];
    setFlag(CF, dst < src);
    setFlag(AF, ((res ^ dst ^ src) & AF) > 0);
    setFlag(OF, (shift((dst ^ src) & (dst ^ res), 12 - BITS[w]) & OF) > 0);
    setFlags(w, res);
    return res;
}
void Intel8086::callInt(int type)
{
    push(flags);
    setFlag(IF, false);
    setFlag(TF, false);
    push(cs);
    push(ip);
    ip = getMem(0b1, type * 4);
    cs = getMem(0b1, type * 4 + 2);
}
int Intel8086::dec(int w, int dst)
{
    int res = dst - 1 & MASK[w];
    setFlag(AF, ((res ^ dst ^ 1) & AF) > 0);
    setFlag(OF, res == SIGN[w] - 1);
    setFlags(w, res);
    return res;
}
void Intel8086::decode()
{
    mod = static_cast<int>(static_cast<unsigned int>(queue[1]) >> 6) & 0b11;
    reg = static_cast<int>(static_cast<unsigned int>(queue[1]) >> 3) & 0b111;
    rm  = queue[1] & 0b111;
    if (mod == 0b01) {
        ip = ip + 2 & 0xffff;
    } else if (mod == 0b00 && rm == 0b110 || mod == 0b10) {
        ip = ip + 3 & 0xffff;
    } else {
        ip = ip + 1 & 0xffff;
    }
}
int Intel8086::getAddr(int seg, int off)
{
    return (seg << 4) + off;
}
int Intel8086::getEA(int mod, int rm)
{
    int disp = 0;
    if (mod == 0b01) {
        // 8-bit displacement follows
        clocks += 4;
        disp = queue[2];
    } else if (mod == 0b10) {
        // 16-bit displacement follows
        clocks += 4;
        disp = queue[3] << 8 | queue[2];
    }
    int ea = 0;
    switch (rm) {
        case 0b000:    // EA = (BX) + (SI) + DISP
            clocks += 7;
            ea = bh << 8 | bl + si + disp;
            break;
        case 0b001:    // EA = (BX) + (DI) + DISP
            clocks += 8;
            ea = bh << 8 | bl + di + disp;
            break;
        case 0b010:    // EA = (BP) + (SI) + DISP
            clocks += 8;
            ea = bp + si + disp;
            break;
        case 0b011:    // EA = (BP) + (DI) + DISP
            clocks += 7;
            ea = bp + di + disp;
            break;
        case 0b100:    // EA = (SI) + DISP
            clocks += 5;
            ea = si + disp;
            break;
        case 0b101:    // EA = (DI) + DISP
            clocks += 5;
            ea = di + disp;
            break;
        case 0b110:
            if (mod == 0b00) {
                // Direct address
                clocks += 6;
                ea = queue[3] << 8 | queue[2];
            } else {
                // EA = (BP) + DISP
                clocks += 5;
                ea = bp + disp;
            }
            break;
        case 0b111:    // EA = (BX) + DISP
            clocks += 5;
            ea = bh << 8 | bl + disp;
            break;
    }
    return (os << 4) + (ea & 0xffff);
}

int Intel8086::getMem(int w)
{
    int addr = getAddr(cs, ip);
    int val  = m_memory[addr];
    if (w == W) {
        val |= m_memory[addr + 1] << 8;
    }
    ip = ip + 1 + w & 0xffff;
    return val;
}
int Intel8086::getMem(int w, int addr)
{
    int val = m_memory[addr];
    if (w == W) {
        if ((addr & 0b1) == 0b1) {
            clocks += 4;
        }
        val |= m_memory[addr + 1] << 8;
    }
    return val;
}
int Intel8086::getReg(int w, int reg)
{
    if (w == B) {
        switch (reg) {
            case 0b000:
                return al;
            case 0b001:
                return cl;
            case 0b010:
                return dl;
            case 0b011:
                return bl;
            case 0b100:
                return ah;
            case 0b101:
                return ch;
            case 0b110:
                return dh;
            case 0b111:
                return bh;
        }
    } else {
        switch (reg) {
            case 0b000:
                return ah << 8 | al;
            case 0b001:
                return ch << 8 | cl;
            case 0b010:
                return dh << 8 | dl;
            case 0b011:
                return bh << 8 | bl;
            case 0b100:
                return sp;
            case 0b101:
                return bp;
            case 0b110:
                return si;
            case 0b111:
                return di;
        }
    }
    return 0;
}
int Intel8086::getRM(int w, int mod, int rm)
{
    if (mod == 0b11) {
        return getReg(w, rm);
    } else {
        return getMem(w, ea > 0 ? ea : getEA(mod, rm));
    }
}
int Intel8086::getSegReg(int reg)
{
    switch (reg) {
        case 0b00:
            return es;
        case 0b01:
            return cs;
        case 0b10:
            return ss;
        case 0b11:
            return ds;
    }
    return 0;
}
bool Intel8086::getFlag(int flag)
{
    return (flags & flag) > 0;
}

void Intel8086::setMem(int w, int addr, int val)
{
    // IBM BIOS and BASIC are ROM.
    if (addr >= 0xf6000) {
        return;
    }
    m_memory[addr] = val & 0xff;
    if (w == W) {
        if ((addr & 0b1) == 0b1) {
            clocks += 4;
        }
        m_memory[addr + 1] = static_cast<int>(static_cast<unsigned int>(val) >> 8) & 0xff;
    }
}
void Intel8086::setReg(int w, int reg, int val)
{
    if (w == B) {
        switch (reg) {
            case 0b000:
                al = val & 0xff;
                break;
            case 0b001:
                cl = val & 0xff;
                break;
            case 0b010:
                dl = val & 0xff;
                break;
            case 0b011:
                bl = val & 0xff;
                break;
            case 0b100:
                ah = val & 0xff;
                break;
            case 0b101:
                ch = val & 0xff;
                break;
            case 0b110:
                dh = val & 0xff;
                break;
            case 0b111:
                bh = val & 0xff;
                break;
        }
    } else {
        switch (reg) {
            case 0b000:
                al = val & 0xff;
                ah = static_cast<int>(static_cast<unsigned int>(val) >> 8) & 0xff;
                break;
            case 0b001:
                cl = val & 0xff;
                ch = static_cast<int>(static_cast<unsigned int>(val) >> 8) & 0xff;
                break;
            case 0b010:
                dl = val & 0xff;
                dh = static_cast<int>(static_cast<unsigned int>(val) >> 8) & 0xff;
                break;
            case 0b011:
                bl = val & 0xff;
                bh = static_cast<int>(static_cast<unsigned int>(val) >> 8) & 0xff;
                break;
            case 0b100:
                sp = val & 0xffff;
                break;
            case 0b101:
                bp = val & 0xffff;
                break;
            case 0b110:
                si = val & 0xffff;
                break;
            case 0b111:
                di = val & 0xffff;
                break;
        }
    }
}
void Intel8086::setRM(int w, int mod, int rm, int val)
{
    if (mod == 0b11) {
        setReg(w, rm, val);
    } else {
        setMem(w, ea > 0 ? ea : getEA(mod, rm), val);
    }
}
void Intel8086::setSegReg(int reg, int val)
{
    switch (reg) {
        case 0b00:
            es = val & 0xffff;
            break;
        case 0b01:
            cs = val & 0xffff;
            break;
        case 0b10:
            ss = val & 0xffff;
            break;
        case 0b11:
            ds = val & 0xffff;
            break;
    }
}
void Intel8086::setFlag(int flag, bool set)
{
    if (set) {
        flags |= flag;
    } else {
        flags &= ~flag;
    }
}
void Intel8086::setFlags(int w, int res)
{
    setFlag(PF, PARITY[res & 0xff] > 0);
    setFlag(ZF, res == 0);
    setFlag(SF, (shift(res, 8 - BITS[w]) & SF) > 0);
}

int Intel8086::inc(int w, int dst)
{
    int res = dst + 1 & MASK[w];
    setFlag(AF, ((res ^ dst ^ 1) & AF) > 0);
    setFlag(OF, res == SIGN[w]);
    setFlags(w, res);
    return res;
}
void Intel8086::logic(int w, int res)
{
    setFlag(CF, false);
    setFlag(OF, false);
    setFlags(w, res);
}
int Intel8086::pop()
{
    int val = getMem(W, getAddr(ss, sp));
    sp      = sp + 2 & 0xffff;
    return val;
}
void Intel8086::push(int val)
{
    sp = sp - 2 & 0xffff;
    setMem(W, getAddr(ss, sp), val);
}
int Intel8086::portIn(int w, int port)
{
    for (auto peripheral : m_peripherals) {
        if (peripheral->isConnected(port)) {
            return peripheral->portIn(w, port);
        }
    }
    return 0;
}
void Intel8086::portOut(int w, int port, int val)
{
    for (auto peripheral : m_peripherals) {
        if (peripheral->isConnected(port)) {
            peripheral->portOut(w, port, val);
            return;
        }
    }
}
void Intel8086::show_info(int op)
{
    switch (op) {
        case 0x88: {
            printf("MOV REG8/MEM8,REG8\n");
        } break;
        case 0x89: {
            printf("MOV REG16/MEM16,REG16\n");
        } break;
        case 0x8a: {
            printf("MOV REG8,REG8/MEM8\n");
        } break;
        case 0x8b: {
            printf("MOV REG16,REG16/MEM16\n");
        } break;
        case 0xc6: {
            printf("MOV REG8/MEM8,IMMED8\n");
        } break;
        case 0xc7: {
            printf("MOV REG16/MEM16,IMMED16\n");
        } break;
        case 0xb0: {
            printf("MOV AL,IMMED8\n");
        } break;
        case 0xb1: {
            printf("MOV CL,IMMED8\n");
        } break;
        case 0xb2: {
            printf("MOV DL,IMMED8\n");
        } break;
        case 0xb3: {
            printf("MOV BL,IMMED8\n");
        } break;
        case 0xb4: {
            printf("MOV AH,IMMED8\n");
        } break;
        case 0xb5: {
            printf("MOV CH,IMMED8\n");
        } break;
        case 0xb6: {
            printf("MOV DH,IMMED8\n");
        } break;
        case 0xb7: {
            printf("MOV BH,IMMED8\n");
        } break;
        case 0xb8: {
            printf("MOV AX,IMMED16\n");
        } break;
        case 0xb9: {
            printf("MOV CX,IMMED16\n");
        } break;
        case 0xba: {
            printf("MOV DX,IMMED16\n");
        } break;
        case 0xbb: {
            printf("MOV BX,IMMED16\n");
        } break;
        case 0xbc: {
            printf("MOV SP,IMMED16\n");
        } break;
        case 0xbd: {
            printf("MOV BP,IMMED16\n");
        } break;
        case 0xbe: {
            printf("MOV SI,IMMED16\n");
        } break;
        case 0xbf: {
            printf("MOV DI,IMMED16\n");
        } break;
        case 0xa0: {
            printf("MOV AL,MEM8\n");
        } break;
        case 0xa1: {
            printf("MOV AX,MEM16\n");
        } break;
        case 0xa2: {
            printf("MOV MEM8,AL\n");
        } break;
        case 0xa3: {
            printf("MOV MEM16,AX\n");
        } break;
        case 0x8c: {
            printf("MOV REG16/MEM16,SEGREG\n");
        } break;
        case 0x8e: {
            printf("MOV SEGREG,REG16/MEM16\n");
        } break;
        case 0x50: {
            printf("PUSH AX\n");
        } break;
        case 0x51: {
            printf("PUSH CX\n");
        } break;
        case 0x52: {
            printf("PUSH DX\n");
        } break;
        case 0x53: {
            printf("PUSH BX\n");
        } break;
        case 0x54: {
            printf("PUSH SP\n");
        } break;
        case 0x55: {
            printf("PUSH BP\n");
        } break;
        case 0x56: {
            printf("PUSH SI\n");
        } break;
        case 0x57: {
            printf("PUSH DI\n");
        } break;
        case 0x06: {
            printf("PUSH ES\n");
        } break;
        case 0x0e: {
            printf("PUSH CS\n");
        } break;
        case 0x16: {
            printf("PUSH SS\n");
        } break;
        case 0x1e: {
            printf("PUSH DS\n");
        } break;
        case 0x58: {
            printf("POP AX\n");
        } break;
        case 0x59: {
            printf("POP CX\n");
        } break;
        case 0x5a: {
            printf("POP DX\n");
        } break;
        case 0x5b: {
            printf("POP BX\n");
        } break;
        case 0x5c: {
            printf("POP SP\n");
        } break;
        case 0x5d: {
            printf("POP BP\n");
        } break;
        case 0x5e: {
            printf("POP SI\n");
        } break;
        case 0x5f: {
            printf("POP DI\n");
        } break;
        case 0x07: {
            printf("POP ES\n");
        } break;
        case 0x0f: {
            printf("POP CS\n");
        } break;
        case 0x17: {
            printf("POP SS\n");
        } break;
        case 0x1f: {
            printf("POP DS\n");
        } break;
        case 0x86: {
            printf("XCHG REG8,REG8/MEM8\n");
        } break;
        case 0x87: {
            printf("XCHG REG16,REG16/MEM16\n");
        } break;
        case 0x91: {
            printf("XCHG AX,CX\n");
        } break;
        case 0x92: {
            printf("XCHG AX,DX\n");
        } break;
        case 0x93: {
            printf("XCHG AX,BX\n");
        } break;
        case 0x94: {
            printf("XCHG AX,SP\n");
        } break;
        case 0x95: {
            printf("XCHG AX,BP\n");
        } break;
        case 0x96: {
            printf("XCHG AX,SI\n");
        } break;
        case 0x97: {
            printf("XCHG AX,DI\n");
        } break;
        case 0xd7: {
            printf("XLAT SOURCE-TABLE\n");
        } break;
        case 0xe4: {
            printf("IN AL,IMMED8\n");
        } break;
        case 0xe5: {
            printf("IN AX,IMMED8\n");
        } break;
        case 0xec: {
            printf("IN AL,DX\n");
        } break;
        case 0xed: {
            printf("IN AX,DX\n");
        } break;
        case 0xe6: {
            printf("OUT AL,IMMED8\n");
        } break;
        case 0xe7: {
            printf("OUT AX,IMMED8\n");
        } break;
        case 0xee: {
            printf("OUT AL,DX\n");
        } break;
        case 0xef: {
            printf("OUT AX,DX\n");
        } break;
        case 0x8d: {
            printf("LEA REG16,MEM16\n");
        } break;
        case 0xc5: {
            printf("LDS REG16,MEM32\n");
        } break;
        case 0xc4: {
            printf("LES REG16,MEM32\n");
        } break;
        case 0x9f: {
            printf("LAHF\n");
        } break;
        case 0x9e: {
            printf("SAHF\n");
        } break;
        case 0x9c: {
            printf("PUSHF\n");
        } break;
        case 0x9d: {
            printf("POPF\n");
        } break;
        case 0x00: {
            printf("ADD REG8/MEM8,REG8\n");
        } break;
        case 0x01: {
            printf("ADD REG16/MEM16,REG16\n");
        } break;
        case 0x02: {
            printf("ADD REG8,REG8/MEM8\n");
        } break;
        case 0x03: {
            printf("ADD REG16,REG16/MEM16\n");
        } break;
        case 0x04: {
            printf("ADD AL,IMMED8\n");
        } break;
        case 0x05: {
            printf("ADD AX,IMMED16\n");
        } break;
        case 0x10: {
            printf("ADC REG8/MEM8,REG8\n");
        } break;
        case 0x11: {
            printf("ADC REG16/MEM16,REG16\n");
        } break;
        case 0x12: {
            printf("ADC REG8,REG8/MEM8\n");
        } break;
        case 0x13: {
            printf("ADC REG16,REG16/MEM16\n");
        } break;
        case 0x14: {
            printf("ADC AL,IMMED8\n");
        } break;
        case 0X15: {
            printf("ADC AX,IMMED16\n");
        } break;
        case 0x40: {
            printf("INC AX\n");
        } break;
        case 0x41: {
            printf("INC CX\n");
        } break;
        case 0x42: {
            printf("INC DX\n");
        } break;
        case 0x43: {
            printf("INC BX\n");
        } break;
        case 0x44: {
            printf("INC SP\n");
        } break;
        case 0x45: {
            printf("INC BP\n");
        } break;
        case 0x46: {
            printf("INC SI\n");
        } break;
        case 0x47: {
            printf("INC DI\n");
        } break;
        case 0x37: {
            printf("AAA\n");
        } break;
        case 0x27: {
            printf("DAA\n");
        } break;
        case 0x28: {
            printf("SUB REG8/MEM8,REG8\n");
        } break;
        case 0x29: {
            printf("SUB REG16/MEM16,REG16\n");
        } break;
        case 0x2a: {
            printf("SUB REG8,REG8/MEM8\n");
        } break;
        case 0x2b: {
            printf("SUB REG16,REG16/MEM16\n");
        } break;
        case 0x2c: {
            printf("SUB AL,IMMED8\n");
        } break;
        case 0x2d: {
            printf("SUB AX,IMMED16\n");
        } break;
        case 0x18: {
            printf("SBB REG8/MEM8,REG8\n");
        } break;
        case 0x19: {
            printf("SBB REG16/MEM16,REG16\n");
        } break;
        case 0x1a: {
            printf("SBB REG8,REG8/MEM8\n");
        } break;
        case 0x1b: {
            printf("SBB REG16,REG16/MEM16\n");
        } break;
        case 0x1c: {
            printf("SBB AL,IMMED8\n");
        } break;
        case 0X1d: {
            printf("SBB AX,IMMED16\n");
        } break;
        case 0x48: {
            printf("DEC AX\n");
        } break;
        case 0x49: {
            printf("DEC CX\n");
        } break;
        case 0x4a: {
            printf("DEC DX\n");
        } break;
        case 0x4b: {
            printf("DEC BX\n");
        } break;
        case 0x4c: {
            printf("DEC SP\n");
        } break;
        case 0x4d: {
            printf("DEC BP\n");
        } break;
        case 0x4e: {
            printf("DEC SI\n");
        } break;
        case 0x4f: {
            printf("DEC DI\n");
        } break;
        case 0x38: {
            printf("CMP REG8/MEM8,REG8\n");
        } break;
        case 0x39: {
            printf("CMP REG16/MEM16,REG16\n");
        } break;
        case 0x3a: {
            printf("CMP REG8,REG8/MEM8\n");
        } break;
        case 0x3b: {
            printf("CMP REG16,REG16/MEM16\n");
        } break;
        case 0x3c: {
            printf("CMP AL,IMMED8\n");
        } break;
        case 0x3d: {
            printf("CMP AX,IMMED16\n");
        } break;
        case 0x3f: {
            printf("AAS\n");
        } break;
        case 0x2f: {
            printf("DAS\n");
        } break;
        case 0xd4: {
            printf("AAM\n");
        } break;
        case 0xd5: {
            printf("AAD\n");
        } break;
        case 0x98: {
            printf("CBW\n");
        } break;
        case 0x99: {
            printf("CWD\n");
        } break;
        case 0x20: {
            printf("AND REG8/MEM8,REG8\n");
        } break;
        case 0x21: {
            printf("AND REG16/MEM16,REG16\n");
        } break;
        case 0x22: {
            printf("AND REG8,REG8/MEM8\n");
        } break;
        case 0x23: {
            printf("AND REG16,REG16/MEM16\n");
        } break;
        case 0x24: {
            printf("AND AL,IMMED8\n");
        } break;
        case 0x25: {
            printf("AND AX,IMMED16\n");
        } break;
        case 0x08: {
            printf("OR REG8/MEM8,REG8\n");
        } break;
        case 0x09: {
            printf("OR REG16/MEM16,REG16\n");
        } break;
        case 0x0a: {
            printf("OR REG8,REG8/MEM8\n");
        } break;
        case 0x0b: {
            printf("OR REG16,REG16/MEM16\n");
        } break;
        case 0x0c: {
            printf("OR AL,IMMED8\n");
        } break;
        case 0x0d: {
            printf("OR AX,IMMED16\n");
        } break;
        case 0x30: {
            printf("XOR REG8/MEM8,REG8\n");
        } break;
        case 0x31: {
            printf("XOR REG16/MEM16,REG16\n");
        } break;
        case 0x32: {
            printf("XOR REG8,REG8/MEM8\n");
        } break;
        case 0x33: {
            printf("XOR REG16,REG16/MEM16\n");
        } break;
        case 0x34: {
            printf("XOR AL,IMMED8\n");
        } break;
        case 0x35: {
            printf("XOR AX,IMMED16\n");
        } break;
        case 0x84: {
            printf("TEST REG8/MEM8,REG8\n");
        } break;
        case 0x85: {
            printf("TEST REG16/MEM16,REG16\n");
        } break;
        case 0xa8: {
            printf("TEST AL,IMMED8\n");
        } break;
        case 0xa9: {
            printf("TEST AX,IMMED16\n");
        } break;
        case 0xa4: {
            printf("MOVS DEST-STR8,SRC-STR8\n");
        } break;
        case 0xa5: {
            printf("MOVS DEST-STR16,SRC-STR16\n");
        } break;
        case 0xa6: {
            printf("CMPS DEST-STR8,SRC-STR8\n");
        } break;
        case 0xa7: {
            printf("CMPS DEST-STR16,SRC-STR16\n");
        } break;
        case 0xae: {
            printf("SCAS DEST-STR8\n");
        } break;
        case 0xaf: {
            printf("SCAS DEST-STR16\n");
        } break;
        case 0xac: {
            printf("LODS SRC-STR8\n");
        } break;
        case 0xad: {
            printf("LODS SRC-STR16\n");
        } break;
        case 0xaa: {
            printf("STOS DEST-STR8\n");
        } break;
        case 0xab: {
            printf("STOS DEST-STR16\n");
        } break;
        case 0xe8: {
            printf("CALL NEAR-PROC\n");
        } break;
        case 0x9a: {
            printf("CALL FAR-PROC\n");
        } break;
        case 0xc3: {
            printf("RET (intrasegment)\n");
        } break;
        case 0xc2: {
            printf("RET IMMED16 (intraseg)\n");
        } break;
        case 0xcb: {
            printf("RET (intersegment)\n");
        } break;
        case 0xca: {
            printf("RET IMMED16 (intersegment)\n");
        } break;
        case 0xe9: {
            printf("JMP NEAR-LABEL\n");
        } break;
        case 0xeb: {
            printf("JMP SHORT-LABEL\n");
        } break;
        case 0xea: {
            printf("JMP FAR-LABEL\n");
        } break;
        case 0x70: {
            printf("JO SHORT-LABEL\n");
        } break;
        case 0x71: {
            printf("JNO SHORT-LABEL\n");
        } break;
        case 0x72: {
            printf("JB/JNAE/JC SHORT-LABEL\n");
        } break;
        case 0x73: {
            printf("JNB/JAE/JNC SHORT-LABEL\n");
        } break;
        case 0x74: {
            printf("JE/JZ SHORT-LABEL\n");
        } break;
        case 0x75: {
            printf("JNE/JNZ SHORT-LABEL\n");
        } break;
        case 0x76: {
            printf("JBE/JNA SHORT-LABEL\n");
        } break;
        case 0x77: {
            printf("JNBE/JA SHORT-LABEL\n");
        } break;
        case 0x78: {
            printf("JS SHORT-LABEL\n");
        } break;
        case 0x79: {
            printf("JNS SHORT-LABEL\n");
        } break;
        case 0x7a: {
            printf("JP/JPE SHORT-LABEL\n");
        } break;
        case 0x7b: {
            printf("JNP/JPO SHORT-LABEL\n");
        } break;
        case 0x7c: {
            printf("JL/JNGE SHORT-LABEL\n");
        } break;
        case 0x7d: {
            printf("JNL/JGE SHORT-LABEL\n");
        } break;
        case 0x7e: {
            printf("JLE/JNG SHORT-LABEL\n");
        } break;
        case 0x7f: {
            printf("JNLE/JG SHORT-LABEL\n");
        } break;
        case 0xe2: {
            printf("LOOP SHORT-LABEL\n");
        } break;
        case 0xe1: {
            printf("LOOPE/LOOPZ SHORT-LABEL\n");
        } break;
        case 0xe0: {
            printf("LOOPNE/LOOPNZ SHORT-LABEL\n");
        } break;
        case 0xe3: {
            printf("JCXZ SHORT-LABEL\n");
        } break;
        case 0xcc: {
            printf("INT 3\n");
        } break;
        case 0xcd: {
            printf("INT IMMED8\n");
        } break;
        case 0xce: {
            printf("INTO\n");
        } break;
        case 0xcf: {
            printf("IRET\n");
        } break;
        case 0xf8: {
            printf("CLC\n");
        } break;
        case 0xf5: {
            printf("CMC\n");
        } break;
        case 0xf9: {
            printf("STC\n");
        } break;
        case 0xfc: {
            printf("CLD\n");
        } break;
        case 0xfd: {
            printf("STD\n");
        } break;
        case 0xfa: {
            printf("CLI\n");
        } break;
        case 0xfb: {
            printf("STI\n");
        } break;
        case 0xf4: {
            printf("HLT\n");
        } break;
        case 0x9b: {
            printf("WAIT\n");
        } break;
        case 0xd8: {
            printf("ESC 0,SOURCE\n");
        } break;
        case 0xd9: {
            printf("ESC 1,SOURCE\n");
        } break;
        case 0xda: {
            printf("ESC 2,SOURCE\n");
        } break;
        case 0xdb: {
            printf("ESC 3,SOURCE\n");
        } break;
        case 0xdc: {
            printf("ESC 4,SOURCE\n");
        } break;
        case 0xdd: {
            printf("ESC 5,SOURCE\n");
        } break;
        case 0xde: {
            printf("ESC 6,SOURCE\n");
        } break;
        case 0xdf: {
            printf("ESC 7,SOURCE\n");
        } break;
        case 0xf0: {
            printf("LOCK\n");
        } break;
        case 0x90: {
            printf("NOP\n");
        } break;
        // case 0x80:
        // case 0x81:
        // case 0x82:
        // case 0x83:
        //     //case 0b000: // ADD
        //     //case 0b001: // OR
        //     //case 0b010: // ADC
        //     //case 0b011: // SBB
        //     //case 0b100: // AND
        //     //case 0b101: // SUB
        //     //case 0b110: // XOR
        //     //case 0b111: // CMP
        // case 0x8f: // POP REG16/MEM16
        //     //case 0b000: // POP
        // case 0xd0:
        // case 0xd1:
        // case 0xd2:
        // case 0xd3:
        //     //case 0b000: // ROL
        //     //case 0b001: // ROR
        //     //case 0b010: // RCL
        //     //case 0b011: // RCR
        //     //case 0b100: // SAL/SHL
        //     //case 0b101: // SHR
        //     //case 0b111: // SAR
        // case 0xf6:
        // case 0xf7:
        //     //case 0b000: // TEST
        //     //case 0b010: // NOT
        //     //case 0b011: // NEG
        //     //case 0b100: // MUL
        //     //case 0b101: // IMUL
        //     //case 0b110: // DIV
        //     //case 0b111: // IDIV
        // case 0xfe:
        //     //case 0b000: // INC
        //     //case 0b001: // DEC
        // case 0xff:
        //     //case 0b000: // INC
        //     //case 0b001: // DEC
        //     //case 0b010: // CALL
        //     //case 0b011: // CALL
        //     //case 0b100: // JMP
        //     //case 0b101: // JMP
        //     //case 0b110: // PUSH
        default:
            printf("NOP\n");
            break;
    }
}
