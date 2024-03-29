#ifndef REGISTER_DEF_HPP
#define REGISTER_DEF_HPP

#include <string>
#include "types.hpp"

#define REG_NUM 32

#define REG_RA      1
#define REG_SP      2
#define REG_A0      10
#define REG_A1      11
#define REG_A2      12
#define REG_A3      13
#define REG_A4      14
#define REG_A5      15
#define REG_A6      16
#define REG_A7      17

#define OP_RR       0x33
#define OP_RRW      0x3b
#define OP_LOAD     0x03
#define OP_RI       0x13
#define OP_RIW      0x1b
#define OP_JALR     0x67
#define OP_ECALL    0x73
#define OP_STORE    0x23
#define OP_BRANCH   0x63
#define OP_AUIPC    0x17
#define OP_LUI      0x37
#define OP_JAL      0x6f

enum ALU_OP
{
    ALU_ADD = 0,
    ALU_SUB,
    ALU_MUL,
    ALU_MULH,
    ALU_MULHSU,
    ALU_MULHU,
    ALU_DIV,
    ALU_DIVU,
    ALU_REM,
    ALU_REMU,
    ALU_SLL,
    ALU_SRA,
    ALU_SRL,
    ALU_XOR,
    ALU_OR,
    ALU_AND,
    ALU_SLT,
    ALU_SLTU,
    N_ALU_OP
};

struct PipeReg
{
    bool stall, bubble;
    std::string asm_str;

    void print_inst();
};

struct IFReg : public PipeReg
{
    reg_t predPC;

    void update(const IFReg& r);
    void print();
};

struct IDReg : public PipeReg
{
    inst_t inst;
    reg_t pc;

    void update(const IDReg& r);
    void print();
};

struct EXReg : public PipeReg
{
    bool compressed_inst;
    uint8_t opcode, funct3;
    reg_num_t rs1, rs2, rd;
    ALU_OP alu_op;
    reg_t val1, val2, imm;
    reg_t pc;

    void update(const EXReg& r);
    void print();
};

struct MEMReg : public PipeReg
{
    uint8_t opcode, funct3;
    reg_num_t rd;
    bool cond;
    reg_t valE, val2;
    reg_t pc;

    void update(const MEMReg& r);
    void print();
};

struct WBReg : public PipeReg
{
    uint8_t opcode;
    reg_num_t rd;
    reg_t val;

    void update(const WBReg& r);
    void print();
};

#endif
