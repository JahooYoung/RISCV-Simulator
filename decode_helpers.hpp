#ifndef DECODE_HELPERS_HPP
#define DECODE_HELPERS_HPP

#include <map>
#include "register_def.hpp"

inline uint32_t getbits(inst_t inst, int start, int count)
{
    return (inst >> start) & ((1 << count) - 1);
}

inline void parse_R_Type(inst_t inst, EXReg& e, uint8_t& funct7)
{
    // R-TYPE funct7 rs2 rs1 funct3 rd opcode
    // Bits     7     5   5    3     5    7
    // Sums  32    25   20  15   12    7     0
    e.rd = (uint8_t)getbits(inst, 7, 5);
    e.funct3 = (uint8_t)getbits(inst, 12, 3);
    e.rs1 = (uint8_t)getbits(inst, 15, 5);
    e.rs2 = (uint8_t)getbits(inst, 20, 5);
    funct7 = (uint8_t)getbits(inst, 25, 7);
}

inline void parse_I_Type(inst_t inst, EXReg& e, uint8_t& funct7)
{
    // I-TYPE imm[11:0] rs1 funct3 rd opcode
    // Bits     12       5     3    5    7
    // Sums  32      20     15    12  7     0
    e.rd = (uint8_t)getbits(inst, 7, 5);
    e.funct3 = (uint8_t)getbits(inst, 12, 3);
    e.rs1 = (uint8_t)getbits(inst, 15, 5);
    e.imm = getbits(inst, 20, 12);
    funct7 = (uint8_t)getbits(inst, 25, 7);
}

inline void parse_S_Type(inst_t inst, EXReg& e)
{
    // S-TYPE imm[11:5] rs2 rs1 funct3 imm[4:0] opcode
    // Bits      7       5   5    3       5        7
    // Sums  32      25    20   15    12        7     0
    e.funct3 = (uint8_t)getbits(inst, 12, 3);
    e.rs1 = (uint8_t)getbits(inst, 15, 5);
    e.imm = (getbits(inst, 25, 7) << 5) | getbits(inst, 7, 5);
}

inline void parse_SB_Type(inst_t inst, EXReg& e)
{
    // SB-TYPE imm[12] imm[10:5] rs2 rs1 funct3 imm[4:1] imm[11] opcode
    // Bits      1         6      5   5    3       4       1       7
    // Sums  32      31        25   20  15    12       8         7     0
    e.funct3 = (uint8_t)getbits(inst, 12, 3);
    e.rs1 = (uint8_t)getbits(inst, 15, 5);
    e.rs2 = (uint8_t)getbits(inst, 20, 5);
    e.imm = (getbits(inst, 31, 1) << 12) | (getbits(inst, 25, 6) << 5) |
            (getbits(inst, 8, 4) << 1) | (getbits(inst, 7, 1) << 11);
}

inline void parse_U_Type(inst_t inst, EXReg& e)
{
    // U-TYPE imm[31:12] rd opcode
    // Bits      20      5     7
    // Sums  32      12  7     0
    e.rd = (uint8_t)getbits(inst, 7, 5);
    e.imm = getbits(inst, 12, 20) << 12;
}

inline void parse_UJ_Type(inst_t inst, EXReg& e)
{
    // UJ-TYPE imm[20] imm[10:1] imm[11] imm[19:12] rd opcode
    // Bits      1        10       1         8       5   7
    // Sums  32      31        21      20        12    7     0
    e.rd = (uint8_t)getbits(inst, 7, 5);
    e.imm = (getbits(inst, 31, 1) << 20) | (getbits(inst, 21, 10) << 1) |
            (getbits(inst, 20, 1) << 11) | (getbits(inst, 12, 8) << 12);
}

const std::map<uint8_t, std::map<uint8_t, ALU_OP>> R_alu_op_map = {
    {0x0, {
        {0x00, ALU_ADD},
        {0x01, ALU_MUL},
        {0x20, ALU_SUB},
    }},
    {0x1, {
        {0x00, ALU_SLL},
        {0x01, ALU_MUL},
    }},
    {0x2, {
        {0x00, ALU_SLT},
    }},
    {0x4, {
        {0x00, ALU_XOR},
        {0x01, ALU_DIV},
    }},
    {0x5, {
        {0x00, ALU_SRL},
        {0x20, ALU_SRA},
    }},
    {0x6, {
        {0x00, ALU_OR},
        {0x01, ALU_REM},
    }},
    {0x7, {
        {0x00, ALU_AND},
    }},
};

#endif
