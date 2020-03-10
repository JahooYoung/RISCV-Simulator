#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <nlohmann/json.hpp>
#include "register_def.hpp"
#include "elf_reader.hpp"
#include "branch_predictor.hpp"

class Simulator
{
private:
    ElfReader elf_reader;

    bool single_step;

    MemorySystem mem_sys;
    BranchPredictor *br_pred;
    reg_t reg[32];

    // uppercase refer to pipeline registers, lowercase refer to
    // the signal to be written to the corresponding registers
    IFReg F, f;
    IDReg D, d;
    EXReg E, e;
    MEMReg M, m;
    WBReg W, w;

    void IF();
    void ID();
    void EX();
    void MEM();
    void WB();

public:
    Simulator(const nlohmann::json& config);
    ~Simulator();
    void run();
};

#endif
