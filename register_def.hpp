#ifndef REGISTER_DEF_HPP
#define REGISTER_DEF_HPP

#include "types.hpp"

enum ALU_OP
{
    ALU_ADD,
    ALU_SUB,
    ALU_MUL,
    ALU_MULH,
    ALU_DIV,
    ALU_REM,
    ALU_SLL,
    ALU_SRA,
    ALU_SRL,
    ALU_XOR,
    ALU_OR,
    ALU_AND,
    ALU_SLT
};

enum MEM_OP
{
    NOP = 0,
    READ,
    WRITE
};

struct PipeReg
{
    bool bubble;
};

struct IFReg : public PipeReg
{
    reg_t predPC;
};

struct IDReg : public PipeReg
{
    inst_t inst;
    reg_t valP;
};

struct EXReg : public PipeReg
{
    reg_t val1, val2;
    uint32_t imm;
    ALU_OP alu_op;
    uint8_t rs1, rs2, rd;
    uint8_t funct3;
    MEM_OP mem_op;
};

struct MEMReg : public PipeReg
{
    MEM_OP mem_op;
    reg_t valE;
    uint8_t rd;
    uint8_t funct3;
};

struct WBReg : public PipeReg
{
    reg_t val;
    uint8_t rd;
};

#endif
