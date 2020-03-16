#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <set>
#include <nlohmann/json.hpp>
#include "register_def.hpp"
#include "elf_reader.hpp"
#include "branch_predictor.hpp"

#define REG_NUM 32

class Simulator
{
private:
    // configuration
    bool disasemble;
    bool single_step;
    bool data_forwarding;
    bool verbose;

    // elf file related
    ElfReader elf_reader;
    InstructionMap inst_map;

    MemorySystem mem_sys;
    BranchPredictor *br_pred;
    reg_t reg[REG_NUM];
    size_t tick;

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

    // debug related
    bool running;
    bool stepping;
    std::set<uintptr_t> breakpoints;

    void print_regs();
    void print_pipeline();
    bool check_breakpoint(reg_t pc);
    int process_command();

public:
    Simulator(const nlohmann::json& config);
    ~Simulator();
    void run_prog();
    void start();
};

#endif
