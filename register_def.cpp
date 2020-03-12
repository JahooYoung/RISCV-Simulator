#include <cstdio>
#include "register_def.hpp"
using namespace std;

void PipeReg::print_inst()
{
    if (bubble)
        printf("         bubble\n");
    else
        printf("%s\n", asm_str.c_str());
}

template<typename T>
inline void _update(T& R, const T& r)
{
    if (R.stall) {
        R.stall = false;
        return;
    }
    if (r.bubble) {
        R = {};
        R.bubble = true;
        return;
    }
    R = r;
}

void IFReg::update(const IFReg& r)
{
    _update(*this, r);
}

void IFReg::print()
{
    printf("        predPC=%lx\n", predPC);
}

void IDReg::update(const IDReg& r)
{
    _update(*this, r);
}

void IDReg::print()
{
    printf("        inst=0x%x valP=0x%lx predPC=0x%lx\n", inst, valP, predPC);
}

void EXReg::update(const EXReg& r)
{
    _update(*this, r);
}

void EXReg::print()
{
    printf("        opcode=0x%02x funct3=0x%1x comp=%d predPC=0x%lx\n",
        opcode, funct3, (int)compressed_inst, predPC);
    printf("        rd=%d rs1=%d rs2=%d alu_op=%d\n", rd, rs1, rs2, alu_op);
    printf("        val1=%lx val2=%lx imm=%lx valP=%lx\n", val1, val2, imm, valP);
}

void MEMReg::update(const MEMReg& r)
{
    _update(*this, r);
}

void MEMReg::print()
{
    printf("        opcode=0x%02x funct3=0x%1x predPC=0x%lx\n", opcode, funct3, predPC);
    printf("        cond=%d rd=%d\n", (int)cond, rd);
    printf("        valE=%lx val2=%lx\n", valE, val2);
}

void WBReg::update(const WBReg& r)
{
    _update(*this, r);
}

void WBReg::print()
{
    printf("        rd=%d val=%lx\n", rd, val);
}
