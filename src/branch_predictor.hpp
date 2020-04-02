#ifndef BRANCH_PREDICTOR_HPP
#define BRANCH_PREDICTOR_HPP

#include "types.hpp"

struct BranchPredictor
{
    virtual const char* get_name() const = 0;
    virtual reg_t predict(reg_t next_pc, reg_t target) const = 0;
    virtual void feedback(reg_t next_pc, bool taken) {};
    virtual ~BranchPredictor() = default;
};

struct NeverTaken : public BranchPredictor
{
    const char* get_name() const;
    reg_t predict(reg_t next_pc, reg_t target) const;
};

struct AlwaysTaken : public BranchPredictor
{
    const char* get_name() const;
    reg_t predict(reg_t next_pc, reg_t target) const;
};

// Backward taken, forward not taken
struct BTFNT : public BranchPredictor
{
    const char* get_name() const;
    reg_t predict(reg_t next_pc, reg_t target) const;
};

#define BHT_SIZE (1 << 12)

struct BranchHistoryTable : public BranchPredictor
{
    uint8_t bht[BHT_SIZE];
    BranchHistoryTable();
    const char* get_name() const;
    reg_t predict(reg_t next_pc, reg_t target) const;
    void feedback(reg_t next_pc, bool taken);
};

#endif
