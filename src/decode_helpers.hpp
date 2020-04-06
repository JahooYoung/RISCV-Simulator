#ifndef DECODE_HELPERS_HPP
#define DECODE_HELPERS_HPP

#include "register_def.hpp"

inline reg_t sign_extend(reg_t reg, int bits)
{
    if (bits == 64 || ((reg >> (bits - 1)) & 1) == 0)
        return reg;
    else
        return reg | (-1ULL << bits);
    // reg = (-1ULL << bits) | reg;
    // reg += (((reg >> (bits - 1)) & 1) ^ 1) << bits;
    // return reg;
}

inline reg_t zero_extend(reg_t reg, int bits)
{
    return reg & (-1ULL >> (64 - bits));
}

void parse_inst(inst_t inst, EXReg& e);

#endif
