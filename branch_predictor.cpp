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
    parse_inst(inst, e);
    switch (e.opcode) {
    case OP_BRANCH:
    case OP_JAL:
        return pc + e.imm;
    default:
        return pc + ((inst & 3) != 3 ? 2 : 4);
    }
}

reg_t BTFNT::predict(reg_t pc, inst_t inst)
{
    EXReg e;
    parse_inst(inst, e);
    switch (e.opcode) {
    case OP_BRANCH:
    case OP_JAL:
        if ((int64_t)e.imm < 0)
            return pc + e.imm;
        /* fall through */
    default:
        return pc + ((inst & 3) != 3 ? 2 : 4);
    }
}
