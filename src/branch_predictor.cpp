#include "branch_predictor.hpp"
using namespace std;

const char* NeverTaken::get_name() const
{
    return "never taken";
}

reg_t NeverTaken::predict(reg_t next_pc, reg_t target) const
{
    return next_pc;
}

const char *AlwaysTaken::get_name() const
{
    return "always taken";
}

reg_t AlwaysTaken::predict(reg_t next_pc, reg_t target) const
{
    return target;
}

const char *BTFNT::get_name() const
{
    return "backward taken, forward not taken";
}

reg_t BTFNT::predict(reg_t next_pc, reg_t target) const
{
    return min(next_pc, target);
}

BranchHistoryTable::BranchHistoryTable()
{
    memset(bht, 3, sizeof(bht));
}

const char* BranchHistoryTable::get_name() const
{
    return "2-bit branch history table";
}

reg_t BranchHistoryTable::predict(reg_t next_pc, reg_t target) const
{
    return bht[(next_pc >> 1) % BHT_SIZE] >= 2 ? target : next_pc;
}

void BranchHistoryTable::feedback(reg_t next_pc, bool taken)
{
    auto& bits = bht[(next_pc >> 1) % BHT_SIZE];
    if (taken && bits != 3)
        bits++;
    else if (!taken && bits != 0)
        bits--;
}
