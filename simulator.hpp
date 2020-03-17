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
    int stack_size;

    // elf file related
    ElfReader elf_reader;
    InstructionMap inst_map;

    reg_t reg[REG_NUM];
    MemorySystem mem_sys;
    BranchPredictor *br_pred;
    size_t tick;
    size_t instruction_count;

    // uppercase refer to pipeline registers, lowercase refer to
    // the signal to be written to the corresponding registers
    IFReg F, f;
    IDReg D, d;
    EXReg E, e;
    MEMReg M, m;
    WBReg W, w;

    int IF();
    int ID();
    int EX();
    int MEM();
    int WB();

    int process_syscall();
    void run_prog();

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
    void start();
};

#endif
