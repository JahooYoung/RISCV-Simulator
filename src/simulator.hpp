#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <set>
#include <sstream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <syscall.h>
#include "register_def.hpp"
#include "elf_reader.hpp"
#include "branch_predictor.hpp"

using ArgumentVector = std::vector<std::string>;

class Simulator
{
private:
    // configuration
    bool disassemble;
    bool single_step;
    bool data_forwarding;
    bool verbose;
    int stack_size;
    int alu_cycles[N_ALU_OP];
    int ecall_cycles[NSYSCALLS];
    BranchPredictor *br_pred;

    // elf file related
    ElfReader elf_reader;
    InstructionMap inst_map;
    ArgumentVector argv;

    // performace count
    size_t tick;
    size_t instruction_count;

    // uppercase refer to pipeline registers, lowercase refer to
    // the signal to be written to the corresponding registers
    IFReg F, f;
    IDReg D, d;
    EXReg E, e;
    MEMReg M, m;
    WBReg W, w;

    reg_t reg[REG_NUM];
    MemorySystem mem_sys;
    std::stringstream input_buffer;

    int IF();
    reg_t select_value(reg_num_t rs);
    int ID();
    int EX();
    int MEM();
    int WB();
    int process_syscall();
    void process_control_signal();
    void init_stack();
    void run_prog();

    // debug related
    bool running;
    bool stepping;
    std::set<uintptr_t> breakpoints;
    enum cmd_num_t {
        CMD_CONTINUE,
        CMD_RUN,
        CMD_KILL
    };

    void print_regs();
    void print_pipeline();
    bool check_breakpoint(reg_t pc);
    uint64_t evaluate(const std::string& exp);
    cmd_num_t process_command();

public:
    Simulator(const nlohmann::json& config, ArgumentVector&& argv);
    ~Simulator();
    void start();
};

#endif
