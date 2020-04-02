#include <map>
#include <tuple>
#include "decode_helpers.hpp"
using namespace std;

static const map<uint8_t, map<uint8_t, ALU_OP>> R_alu_op_map = {
    {0x0, {
        {0x00, ALU_ADD},
        {0x01, ALU_MUL},
        {0x20, ALU_SUB},
    }},
    {0x1, {
        {0x00, ALU_SLL},
        {0x01, ALU_MULH},
    }},
    {0x2, {
        {0x00, ALU_SLT},
        {0x01, ALU_MULHSU},
    }},
    {0X3, {
        {0x00, ALU_SLTU},
        {0X01, ALU_MULHU},
    }},
    {0x4, {
        {0x00, ALU_XOR},
        {0x01, ALU_DIV},
    }},
    {0x5, {
        {0x00, ALU_SRL},
        {0x01, ALU_DIVU},
        {0x20, ALU_SRA},
    }},
    {0x6, {
        {0x00, ALU_OR},
        {0x01, ALU_REM},
    }},
    {0x7, {
        {0x00, ALU_AND},
        {0X01, ALU_REMU},
    }},
};

static const map<uint8_t, map<uint8_t, ALU_OP>> I_alu_op_map = {
    {0x0, {
        {0x00, ALU_ADD},
    }},
    {0x1, {
        {0x00, ALU_SLL},
    }},
    {0x2, {
        {0x00, ALU_SLT},
    }},
    {0x3, {
        {0x00, ALU_SLTU},
    }},
    {0x4, {
        {0x00, ALU_XOR},
    }},
    {0x5, {
        {0x00, ALU_SRL},
        {0x10, ALU_SRA},
    }},
    {0x6, {
        {0x00, ALU_OR},
    }},
    {0x7, {
        {0x00, ALU_AND},
    }},
};

static const map<uint8_t, tuple<uint8_t, ALU_OP>> C_R_op_map = {
    {0x0 << 2 | 0, {0x33, ALU_SUB}},
    {0x1 << 2 | 0, {0x33, ALU_XOR}},
    {0x2 << 2 | 0, {0x33, ALU_OR}},
    {0x3 << 2 | 0, {0x33, ALU_AND}},
    {0x0 << 2 | 1, {0x3b, ALU_SUB}},
    {0x1 << 2 | 1, {0x3b, ALU_ADD}},
};

/**
 *  extract `count` bits from `start` in `inst` and left shift `shamt` bits
 */
inline uint32_t getbits(inst_t inst, int start, int count, int shamt = 0)
{
    return ((inst >> start) & ((1 << count) - 1)) << shamt;
}

inline void parse_16b_inst(inst_t inst, EXReg& e)
{
    uint8_t opcode = (uint8_t)getbits(inst, 0, 2);
    uint8_t funct3 = (uint8_t)getbits(inst, 13, 3);
    uint8_t funct2;
    switch (opcode) {
    case 0x0:
        switch (funct3) {
        case 0x0:  // addi4spn => addi rd', x2, nzuimm[9:2]
            e.opcode = OP_RI;
            e.rd = 8 | getbits(inst, 2, 3);
            e.rs1 = REG_SP;
            e.imm = getbits(inst, 5, 1, 3) | getbits(inst, 6, 1, 2) |
                    getbits(inst, 7, 4, 6) | getbits(inst, 11, 2, 4);
            break;
        case 0x2:  // lw => lw rd', offset[6:2](rs1')
            e.opcode = OP_LOAD;
            e.funct3 = 0x2;
            e.rd = 8 | getbits(inst, 2, 3);
            e.rs1 = 8 | getbits(inst, 7, 3);
            e.imm = getbits(inst, 5, 1, 6) | getbits(inst, 6, 1, 2) |
                    getbits(inst, 10, 3, 3);
            break;
        case 0x3:  // ld => ld rd', offset[7:3](rs1'), zero-extended
            e.opcode = OP_LOAD;
            e.funct3 = 0x3;
            e.rd = 8 | getbits(inst, 2, 3);
            e.rs1 = 8 | getbits(inst, 7, 3);
            e.imm = getbits(inst, 5, 2, 6) | getbits(inst, 10, 3, 3);
            break;
        case 0x6: // sw => sw rs2', offset[6:2](rs1')
            e.opcode = OP_STORE;
            e.funct3 = 0x2;
            e.rs2 = 8 | getbits(inst, 2, 3);
            e.rs1 = 8 | getbits(inst, 7, 3);
            e.imm = getbits(inst, 5, 1, 6) | getbits(inst, 6, 1, 2) |
                    getbits(inst, 10, 3, 3);
            break;
        case 0x7: // sd => sd rs2', offset[7:3](rs1')
            e.opcode = OP_STORE;
            e.funct3 = 0x3;
            e.rs2 = 8 | getbits(inst, 2, 3);
            e.rs1 = 8 | getbits(inst, 7, 3);
            e.imm = getbits(inst, 5, 2, 6) | getbits(inst, 10, 3, 3);
            break;
        default:
            throw_error("unknown compressed inst: %x", inst);
        }
        break;

    case 0x1:
        switch (funct3) {
        case 0x0:  // addi => addi rd, rd, nzimm[5:0]
            // TODO: handle NOP as bubble?
            e.opcode = OP_RI;
            e.imm = getbits(inst, 2, 5, 0) | getbits(inst, 12, 1, 5);
            e.imm = sign_extend(e.imm, 6);
            e.rd = e.rs1 = getbits(inst, 7, 5);
            break;
        case 0x1:  // addiw => addiw rd, rd, imm[5:0]
            e.opcode = OP_RIW;
            e.imm = getbits(inst, 2, 5, 0) | getbits(inst, 12, 1, 5);
            e.imm = sign_extend(e.imm, 6);
            e.rd = e.rs1 = getbits(inst, 7, 5);
            break;
        case 0x2:  // li => addi rd, x0, imm[5:0]
            e.opcode = OP_RI;
            e.imm = getbits(inst, 2, 5, 0) | getbits(inst, 12, 1, 5);
            e.imm = sign_extend(e.imm, 6);
            e.rd = getbits(inst, 7, 5);
            break;
        case 0x3:  // addi16sp, lui
            e.rd = getbits(inst, 7, 5);
            if (e.rd == 2) {
                // addi16sp => addi x2, x2, nzimm[9:4]
                e.opcode = OP_RI;
                e.rs1 = REG_SP;
                e.imm = getbits(inst, 2, 1, 5) | getbits(inst, 3, 2, 7) |
                        getbits(inst, 5, 1, 6) | getbits(inst, 6, 1, 4) |
                        getbits(inst, 12, 1, 9);
                e.imm = sign_extend(e.imm, 10);
            } else {
                // lui => lui rd, nzimm[17:12]
                e.opcode = OP_LUI;
                e.imm = getbits(inst, 2, 5, 12) | getbits(inst, 12, 1, 17);
                e.imm = sign_extend(e.imm, 18);
            }
            break;
        case 0x4:
            funct2 = getbits(inst, 10, 2);
            switch (funct2) {
            case 0x0:  // srli => srli rd', rd', shamt[5:0]
                e.opcode = OP_RI;
                e.funct3 = 0x5;
                e.rd = e.rs1 = 8 | getbits(inst, 7, 3);
                e.imm = getbits(inst, 2, 5, 0) | getbits(inst, 12, 1, 5);
                e.alu_op = ALU_SRL;
                break;
            case 0x1:  // srai => srai rd', rd', shamt[5:0]
                e.opcode = OP_RI;
                e.funct3 = 0x5;
                e.rd = e.rs1 = 8 | getbits(inst, 7, 3);
                e.imm = getbits(inst, 2, 5, 0) | getbits(inst, 12, 1, 5);
                e.alu_op = ALU_SRA;
                break;
            case 0x2:  // andi => andi rd', rd', imm[5:0]
                e.opcode = OP_RI;
                e.funct3 = 0x7;
                e.rd = e.rs1 = 8 | getbits(inst, 7, 3);
                e.imm = getbits(inst, 2, 5, 0) | getbits(inst, 12, 1, 5);
                e.imm = sign_extend(e.imm, 6);
                e.alu_op = ALU_AND;
                break;
            case 0x3:
                e.rd = e.rs1 = 8 | getbits(inst, 7, 3);
                e.rs2 = 8 | getbits(inst, 2, 3);
                try {
                    uint8_t funct = getbits(inst, 5, 2);
                    uint8_t funct1 = getbits(inst, 12, 1);
                    //! Does not maintain e.funct3
                    tie(e.opcode, e.alu_op) = C_R_op_map.at(funct << 2 | funct1);
                } catch (out_of_range) {
                    throw_error("reserved compressed inst");
                }
                break;
            }
            break;
        case 0x5:  // j => jal x0, offset[11:1]
            e.opcode = OP_JAL;
            e.imm = getbits(inst, 2, 1, 5) | getbits(inst, 3, 3, 1) |
                    getbits(inst, 6, 1, 7) | getbits(inst, 7, 1, 6) |
                    getbits(inst, 8, 1, 10) | getbits(inst, 9, 2, 8) |
                    getbits(inst, 11, 1, 4) | getbits(inst, 12, 1, 11);
            e.imm = sign_extend(e.imm, 12);
            break;
        case 0x6:  // beqz => beq rs1', x0, offset[8:1]
        case 0x7:  // bnez => bne rs1', x0, offset[8:1]
            e.opcode = OP_BRANCH;
            e.funct3 = funct3 == 0x6 ? 0x0 : 0x1;
            e.rs1 = 8 | getbits(inst, 7, 3);
            e.imm = getbits(inst, 2, 1, 5) | getbits(inst, 3, 2, 1) |
                    getbits(inst, 5, 2, 6) | getbits(inst, 10, 2, 3) |
                    getbits(inst, 12, 1, 8);
            e.imm = sign_extend(e.imm, 9);
        }
        break;

    case 0x2:
        switch (funct3) {
        case 0x0:  // slli => slli rd, rd, shamt[5:0]
            e.opcode = OP_RI;
            e.funct3 = 0x1;
            e.alu_op = ALU_SLL;
            e.rd = e.rs1 = getbits(inst, 7, 5);
            e.imm = getbits(inst, 12, 1, 5) | getbits(inst, 2, 5);
            break;
        case 0x2:  // lwsp => lw rd, offset[7:2](x2)
            e.opcode = OP_LOAD;
            e.funct3 = 0x2;
            e.imm = getbits(inst, 12, 1, 5) | getbits(inst, 2, 2, 6) |
                    getbits(inst, 4, 3, 2);
            e.rd = getbits(inst, 7, 5);
            e.rs1 = REG_SP;
            break;
        case 0x3:  // ldsp => ld rd, offset[8:3](x2)
            e.opcode = OP_LOAD;
            e.funct3 = 0x3;
            e.imm = getbits(inst, 12, 1, 5) | getbits(inst, 2, 3, 6) |
                    getbits(inst, 5, 2, 3);
            e.rd = getbits(inst, 7, 5);
            e.rs1 = REG_SP;
            break;
        case 0x4:  // jr, jalr, mv, add
            e.rs1 = getbits(inst, 7, 5);
            e.rs2 = getbits(inst, 2, 5);
            switch (getbits(inst, 12, 1)) {
            case 0:
                if (e.rs2 == 0) {
                    // jr => jalr x0, rs1, 0
                    e.opcode = OP_JALR;
                } else {
                    // mv => add r0, x0, rs2
                    e.opcode = OP_RR;
                    swap(e.rd, e.rs1);
                }
                break;
            case 1:
                if (e.rs1 == 0 && e.rs2 == 0) {
                    // ! ebreak
                    // ! I have to accept ebreak since it appear in my malloc
                    // throw_error("ebreak is not supported");
                } else if (e.rs2 == 0) {
                    // jalr => jalr x1, rs1, 0
                    e.opcode = OP_JALR;
                    e.rd = REG_RA;
                } else {
                    // add => add rd, rd, rs2
                    e.opcode = OP_RR;
                    e.rd = e.rs1;
                }
            }
            break;
        case 0x6:  // swsp => sw rs2, offset[7:2](x2)
            e.opcode = 0x23;
            e.funct3 = 0x2;
            e.imm = getbits(inst, 7, 2, 6) | getbits(inst, 9, 4, 2);
            e.rs1 = REG_SP;
            e.rs2 = getbits(inst, 2, 5);
            break;
        case 0x7:  // sdsp => sd rs2, offset[8:3](x2)
            e.opcode = 0x23;
            e.funct3 = 0x3;
            e.imm = getbits(inst, 7, 3, 6) | getbits(inst, 10, 3, 3);
            e.rs1 = REG_SP;
            e.rs2 = getbits(inst, 2, 5);
            break;
        default:
            throw_error("unknown compressed inst: op 0x%02x funct3 0x%02x", opcode, funct3);
        }
    }
}

/**
 * parse 32-bit instruction
 */

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
    e.rs2 = (uint8_t)getbits(inst, 20, 5);
    e.imm = getbits(inst, 25, 7, 5) | getbits(inst, 7, 5);
}

inline void parse_SB_Type(inst_t inst, EXReg& e)
{
    // SB-TYPE imm[12] imm[10:5] rs2 rs1 funct3 imm[4:1] imm[11] opcode
    // Bits      1         6      5   5    3       4       1       7
    // Sums  32      31        25   20  15    12       8         7     0
    e.funct3 = (uint8_t)getbits(inst, 12, 3);
    e.rs1 = (uint8_t)getbits(inst, 15, 5);
    e.rs2 = (uint8_t)getbits(inst, 20, 5);
    e.imm = getbits(inst, 31, 1, 12) | getbits(inst, 25, 6, 5) |
            getbits(inst, 8, 4, 1) | getbits(inst, 7, 1, 11);
}

inline void parse_U_Type(inst_t inst, EXReg& e)
{
    // U-TYPE imm[31:12] rd opcode
    // Bits      20      5     7
    // Sums  32      12  7     0
    e.rd = (uint8_t)getbits(inst, 7, 5);
    e.imm = getbits(inst, 12, 20, 12);
}

inline void parse_UJ_Type(inst_t inst, EXReg& e)
{
    // UJ-TYPE imm[20] imm[10:1] imm[11] imm[19:12] rd opcode
    // Bits      1        10       1         8       5   7
    // Sums  32      31        21      20        12    7     0
    e.rd = (uint8_t)getbits(inst, 7, 5);
    e.imm = getbits(inst, 31, 1, 20) | getbits(inst, 21, 10, 1) |
            getbits(inst, 20, 1, 11) | getbits(inst, 12, 8, 12);
}

inline void parse_32b_inst(inst_t inst, EXReg& e)
{
    static char msg_template[] = "unknown instruction: %08x (opcode 0x%02x funct3 0x%02x funct7 0x%02x)";
    e.opcode = (uint8_t)getbits(inst, 0, 7);
    uint8_t funct7 = 0;
    switch (e.opcode) {
    case OP_RR:  // R-TYPE, Integer Register-Register Operations
        parse_R_Type(inst, e, funct7);
        try {
            e.alu_op = R_alu_op_map.at(e.funct3).at(funct7);
        } catch (out_of_range err) {
            throw_error(msg_template, inst, e.opcode, e.funct3, funct7);
        }
        break;
    case OP_RRW:  // R-TYPE, Integer Register-Register Operations
        parse_R_Type(inst, e, funct7);
        try {
            e.alu_op = R_alu_op_map.at(e.funct3).at(funct7);
        } catch (out_of_range err) {
            throw_error(msg_template, inst, e.opcode, e.funct3, funct7);
        }
        break;
    case OP_LOAD:  // I-TYPE, Load Instructions
        parse_I_Type(inst, e, funct7);
        e.imm = sign_extend(e.imm, 12);
        break;
    case OP_RI:  // I-TYPE, Integer Register-Immediate Instructions
        parse_I_Type(inst, e, funct7);
        if (e.funct3 == 0x1 || e.funct3 == 0x5) {
            funct7 >>= 1;
            e.imm &= 0x3F;
        } else {
            funct7 = 0;
            e.imm = sign_extend(e.imm, 12);
        }
        try {
            e.alu_op = I_alu_op_map.at(e.funct3).at(funct7);
        } catch (out_of_range err) {
            throw_error(msg_template, inst, e.opcode, e.funct3, funct7);
        }
        break;
    case OP_RIW:  // I-TYPE, Integer Register-Immediate Instructions
        parse_I_Type(inst, e, funct7);
        if (e.funct3 == 0x1 || e.funct3 == 0x5) {
            funct7 >>= 1;
            e.imm &= 0x1F;
        } else {
            funct7 = 0;
            e.imm = sign_extend(e.imm, 12);
        }
        try {
            e.alu_op = I_alu_op_map.at(e.funct3).at(funct7);
        } catch (out_of_range err) {
            throw_error(msg_template, inst, e.opcode, e.funct3, funct7);
        }
        break;
    case OP_JALR:  // I-TYPE, jalr
        parse_I_Type(inst, e, funct7);
        e.imm = sign_extend(e.imm, 12);
        break;
    case OP_ECALL:  // I-TYPE, ecall
        // no processing needed
        break;
    case OP_STORE:  // S-TYPE, Store Instructions
        parse_S_Type(inst, e);
        e.imm = sign_extend(e.imm, 12);
        break;
    case OP_BRANCH:  // SB-TYPE, Conditional Branches
        parse_SB_Type(inst, e);
        e.imm = sign_extend(e.imm, 12);
        break;
    case OP_AUIPC:  // U-TYPE, auipc
        parse_U_Type(inst, e);
        e.imm = sign_extend(e.imm, 32);
        break;
    case OP_LUI:  // U-TYPE, lui
        parse_U_Type(inst, e);
        e.imm = sign_extend(e.imm, 32);
        break;
    case OP_JAL:  // UJ-TYPE, jal
        parse_UJ_Type(inst, e);
        e.imm = sign_extend(e.imm, 21);
        break;
    default:
        throw_error(msg_template, inst, e.opcode, 0, 0);
    }
}

void parse_inst(inst_t inst, EXReg& e)
{
    if ((inst & 3) != 3) {
        e.compressed_inst = true;
        parse_16b_inst(inst, e);
    } else {
        e.compressed_inst = false;
        parse_32b_inst(inst, e);
    }
}
