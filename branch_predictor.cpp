#include "branch_predictor.hpp"
#include "decode_helpers.hpp"
using namespace std;

reg_t NeverTaken::predict(reg_t pc, inst_t inst)
{
    return pc + ((inst & 3) != 3 ? 2 : 4);
}

reg_t AlwaysTaken::predict(reg_t pc, inst_t inst)
{
    EXReg e;
    if ((inst & 3) != 3) {
        parse_16b_inst(inst, e);
    } else {
        parse_32b_inst(inst, e);
    }
    switch (e.opcode) {
    case OP_BRANCH:
    case OP_JAL:
        return pc + e.imm;
    default:
        return pc + ((inst & 3) != 3 ? 2 : 4);
    }
}
