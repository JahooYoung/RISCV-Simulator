#ifndef BRANCH_PREDICTOR_HPP
#define BRANCH_PREDICTOR_HPP

#include "types.hpp"

struct BranchPredictor
{
    virtual reg_t predict(reg_t pc, inst_t inst) = 0;
    virtual ~BranchPredictor() = default;
};

struct NeverTaken : public BranchPredictor
{
    reg_t predict(reg_t pc, inst_t inst);
};

struct AlwaysTaken : public BranchPredictor
{
    reg_t predict(reg_t pc, inst_t inst);
};

// Backward taken, forward not taken
struct BTFNT : public BranchPredictor
{
    reg_t predict(reg_t pc, inst_t inst);
};

#endif
