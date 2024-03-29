#include <cstring>
#include <ctime>
#include <csetjmp>
#include <csignal>
#include <string>
#include <iostream>
#include "simulator.hpp"
#include "decode_helpers.hpp"
using namespace std;

struct ExitEvent
{
    reg_t status;
    ExitEvent(reg_t status) : status(status) {}
};

static sigjmp_buf saved_env;


Simulator::Simulator(const YAML::Node& option, const YAML::Node& config, ArgumentVector&& argv)
    : disassemble(config["disassemble"].as<bool>(true)),
    single_step(option["single_step"].as<bool>(false)),
    data_forwarding(config["data_forwarding"].as<bool>(true)),
    verbose(option["verbose"].as<bool>(false)),
    stack_size(config["stack_size"].as<int>(1024)),  // KB
    elf_reader(option["elf_file"].as<string>()),
    argv(argv),
    mem_sys(config["cache"], config["memory_cycles"].as<int>(100)),
    running(false)
{
    // read elf file
    if (option["info_file"])
        elf_reader.output_elf_info(option["info_file"].as<string>());

    if (disassemble)
        elf_reader.load_objdump(config["objdump"].as<string>("riscv64-unknown-elf-objdump"), inst_map);

    // get branch predictor
    string bpred_str = config["branch_predictor"].as<string>("branch_history_table");
    if (bpred_str == "never_taken")
        br_pred = new NeverTaken();
    else if (bpred_str == "always_taken")
        br_pred = new AlwaysTaken();
    else if (bpred_str == "btfnt")
        br_pred = new BTFNT();
    else if (bpred_str == "branch_history_table")
        br_pred = new BranchHistoryTable();
    else {
        cerr << "error: no branch prediction strategy named " << bpred_str << endl;
        exit(EXIT_FAILURE);
    }

    // get alu cycles configuration
    YAML::Node alu_cycles_node = config["alu_cycles"];
    alu_cycles[ALU_ADD] = alu_cycles[ALU_SUB] = alu_cycles_node["add_sub"].as<int>(1);
    alu_cycles[ALU_MUL] = alu_cycles[ALU_MULH] = alu_cycles[ALU_MULHSU] =
        alu_cycles[ALU_MULHU] = alu_cycles_node["mul"].as<int>(3);
    alu_cycles[ALU_DIV] = alu_cycles[ALU_DIVU] = alu_cycles[ALU_REM] =
        alu_cycles[ALU_REMU] = alu_cycles_node["div_rem"].as<int>(30);
    alu_cycles[ALU_SLL] = alu_cycles[ALU_SRA] = alu_cycles[ALU_SRL] =
        alu_cycles[ALU_XOR] = alu_cycles[ALU_OR] = alu_cycles[ALU_AND] =
        alu_cycles_node["bit_op"].as<int>(1);
    alu_cycles[ALU_SLT] = alu_cycles[ALU_SLTU] = alu_cycles_node["slt"].as<int>(1);

    // get ecall cycles configuration
    YAML::Node ecall_cycles_node = config["ecall_cycles"];
    ecall_cycles[SYS_cputchar] = ecall_cycles_node["cputchar"].as<int>(2000);
    ecall_cycles[SYS_sbrk] = ecall_cycles_node["sbrk"].as<int>(1000);
    ecall_cycles[SYS_readint] = ecall_cycles_node["readint"].as<int>(10000);
    ecall_cycles[SYS_time] = ecall_cycles_node["time"].as<int>(1000);
}

Simulator::~Simulator()
{
    delete br_pred;
}

int Simulator::IF()
{
    // select pc
    reg_t pc = F.predPC;
    switch (M.opcode) {
    case OP_BRANCH:
        if (mispredicted)
            pc = M.cond ? M.valE : M.val2;
        break;
    case OP_JALR:
        pc = M.valE;
        break;
    }

    int cycles = mem_sys.read_inst(pc, d.inst);
    d.pc = pc;
    d.asm_str = inst_map[pc];

    // predict pc
    // real hardware implementation only need to identify branch instruction
    EXReg r;
    parse_inst(d.inst, r);
    switch (r.opcode) {
    case OP_BRANCH:
        f.predPC = br_pred->predict(pc + (r.compressed_inst ? 2 : 4), pc + r.imm);
        break;
    case OP_JAL:
        f.predPC = pc + r.imm;
        break;
    case OP_JALR:
    default:
        f.predPC = pc + (r.compressed_inst ? 2 : 4);
    }
    return cycles;
}

inline reg_t Simulator::select_reg_value(reg_num_t rs)
{
    if (data_forwarding && rs != 0) {
        if (E.rd == rs) {
            switch (E.opcode) {
            case OP_JALR:
            case OP_JAL:
            case OP_BRANCH:
                return m.val2;
            default:
                return m.valE;
            }
        } else if (M.rd == rs) {
            return w.val;
        } else if (W.rd == rs) {
            return W.val;
        }
    }
    return reg[rs];
}

int Simulator::ID()
{
    if (D.bubble)
        return 0;

    e.asm_str = D.asm_str;
    e.pc = D.pc;
    // get opcode, funct3, imm, alu_op, rs1, rs2, rd, mem_op, compressed_inst
    parse_inst(D.inst, e);

    // get the register value of rs1 and rs2
    e.val1 = select_reg_value(e.rs1);
    e.val2 = select_reg_value(e.rs2);

    return 1;
}

int Simulator::EX()
{
    if (E.bubble)
        return 0;

    m.asm_str = E.asm_str;
    m.opcode = E.opcode;
    m.funct3 = E.funct3;
    m.rd = E.rd;
    m.pc = E.pc;

    // select m.val2
    switch (E.opcode) {
    case OP_JALR:  // jalr
    case OP_JAL:  // jal
    case OP_BRANCH:  // beq, ...
        m.val2 = E.pc + (E.compressed_inst ? 2 : 4);
        break;
    default:
        m.val2 = E.val2;
    }

    // select valA and valB
    reg_t valA, valB;
    switch (E.opcode) {
    case OP_RR:  // R-TYPE, Integer Register-Register Operations
    case OP_RRW:
        valA = E.val1;
        valB = E.val2;
        break;
    case OP_BRANCH:
    case OP_AUIPC:
    case OP_JAL:
        valA = E.pc;
        valB = E.imm;
        break;
    default:
        valA = E.val1;
        valB = E.imm;
    }

    // run ALU
    switch (E.alu_op) {
    case ALU_ADD: m.valE = valA + valB; break;
    case ALU_SUB: m.valE = valA - valB; break;
    case ALU_MUL: m.valE = valA * valB; break;
    case ALU_MULH: m.valE = ((__int128_t)valA * (__int128_t)valB) >> 64; break;
    case ALU_MULHSU: m.valE = ((__int128_t)valA * (__uint128_t)valB) >> 64; break;
    case ALU_MULHU: m.valE = ((__uint128_t)valA * (__uint128_t)valB) >> 64; break;
    case ALU_DIV: m.valE = (int64_t)valA / (int64_t)valB; break;
    case ALU_DIVU: m.valE = valA / valB; break;
    case ALU_REM: m.valE = (int64_t)valA % (int64_t)valB; break;
    case ALU_REMU: m.valE = valA % valB; break;
    case ALU_SLL: m.valE = valA << valB; break;
    case ALU_SRA: m.valE = (int64_t)valA >> valB; break;
    case ALU_SRL: m.valE = valA >> valB; break;
    case ALU_XOR: m.valE = valA ^ valB; break;
    case ALU_OR: m.valE = valA | valB; break;
    case ALU_AND: m.valE = valA & valB; break;
    case ALU_SLT: m.valE = (int64_t)valA < (int64_t)valB; break;
    case ALU_SLTU: m.valE = valA < valB; break;
    default:
        throw_error("unsupported ALU_OP: %d", E.alu_op);
    }

    // truncate for addw, subw, ...
    if (E.opcode == OP_RIW || E.opcode == OP_RRW)
        m.valE = sign_extend(m.valE, 32);

    // branching
    if (E.opcode == OP_BRANCH) {
        switch (E.funct3) {
        case 0x0: m.cond = E.val1 == E.val2; break;
        case 0x1: m.cond = E.val1 != E.val2; break;
        case 0x4: m.cond = (int64_t)E.val1 < (int64_t)E.val2; break;
        case 0x5: m.cond = (int64_t)E.val1 >= (int64_t)E.val2; break;
        case 0x6: m.cond = E.val1 < E.val2; break;
        case 0x7: m.cond = E.val1 >= E.val2; break;
        }
    }

    return alu_cycles[E.alu_op];
}

int Simulator::MEM()
{
    if (M.bubble)
        return 0;

    w.asm_str = M.asm_str;
    w.opcode = M.opcode;
    w.rd = M.rd;

    int cycles = 1;
    switch (M.opcode) {
    case OP_LOAD:
        if (M.funct3 < 4) {
            cycles = mem_sys.read_data(M.valE, w.val, 1 << M.funct3);
            w.val = sign_extend(w.val, 8 << M.funct3);
        } else {
            cycles = mem_sys.read_data(M.valE, w.val, 1 << (M.funct3 - 4));
            w.val = zero_extend(w.val, 8 << (M.funct3 - 4));
        }
        break;
    case OP_STORE:
        cycles = mem_sys.write_data(M.valE, M.val2, 1 << M.funct3);
        break;
    case OP_JALR:  // jalr
    case OP_JAL:  // jal
    case OP_BRANCH:  // beq, ...
        w.val = M.val2;
        break;
    default:
        w.val = M.valE;
    }

    return cycles;
}

int Simulator::WB()
{
    if (W.bubble)
        return 0;

    if (W.rd != 0)
        reg[W.rd] = W.val;

    return 1;
}

void Simulator::process_control_signal()
{
    mispredicted = false;
    if (E.opcode == OP_BRANCH) {
        // m.val2 == E.pc + (E.compressed_inst ? 2 : 4)
        // m.valE == E.pc + E.imm
        reg_t predPC = br_pred->predict(m.val2, m.valE);
        mispredicted = predPC != (m.cond ? m.valE : m.val2);
        br_pred->feedback(m.val2, m.cond);
    }

    bool meet_jalr = e.opcode == OP_JALR || E.opcode == OP_JALR;

    bool meet_ecall = (E.opcode == OP_ECALL || M.opcode == OP_ECALL || W.opcode == OP_ECALL);
    bool data_dependent = false;
    if (data_forwarding) {
        data_dependent |= (e.rs1 != 0 && e.rs1 == E.rd && E.opcode == OP_LOAD) ||
                          (e.rs2 != 0 && e.rs2 == E.rd && E.opcode == OP_LOAD);
    } else {
        data_dependent |= (e.rs1 != 0 && (E.rd == e.rs1 || M.rd == e.rs1 || W.rd == e.rs1)) ||
                          (e.rs2 != 0 && (E.rd == e.rs2 || M.rd == e.rs2 || W.rd == e.rs2));
    }

    data_dependent |= meet_ecall;
    data_dependent &= !mispredicted;
    meet_jalr &= !mispredicted && !data_dependent;

    W.bubble = M.bubble;
    M.bubble = E.bubble;
    E.bubble = D.bubble || mispredicted || data_dependent;
    D.bubble = mispredicted || meet_jalr;
    D.stall = data_dependent;
    F.stall = data_dependent || meet_jalr;

    mispredicted_time += mispredicted;
    meet_jalr_time += meet_jalr;
    data_dependent_time += data_dependent;
}

void Simulator::init_stack()
{
    int stack_page_num = round_up(stack_size, 4);
    for (int i = 0; i < stack_page_num; i++)
        mem_sys.page_alloc(STACK_TOP - PGSIZE * (i + 1));

    /**
     * stack layout
     *
     * +-------------+  <--- STACK_TOP
     * | arg strings |
     * |     ...     |
     * +-------------+
     * |    NULL     |
     * +-------------+
     * | argv[argc-1]|
     * +-------------+
     * |     ...     |
     * +-------------+
     * |   argv[0]   |
     * +-------------+
     * |    argc     |
     * +-------------+  <--- sp (16 bytes aligned)
     */

    int argc = argv.size();
    size_t length_sum = 0;
    for (auto arg: argv)
        length_sum += arg.size() + 1;
    char *string_store = (char*)STACK_TOP - length_sum;
    uintptr_t *argv_store = (uintptr_t*)round_down(string_store, 8) - argc - 2;
    argv_store = round_down(argv_store, 16) + 1;
    reg[REG_SP] = (uintptr_t)argv_store - 8;
    mem_sys.write_data(reg[REG_SP], argc, 4);
    for (auto arg: argv) {
        mem_sys.write_data((uintptr_t)argv_store, (uintptr_t)string_store, 8);
        mem_sys.write_str((uintptr_t)string_store, arg.c_str());
        string_store += arg.size() + 1;
        argv_store++;
    }
    mem_sys.write_data((uintptr_t)argv_store, 0, 8);
}

void Simulator::run_prog()
{
    memset(reg, 0, sizeof(reg));
    F = {};
    D = {};
    E = {};
    M = {};
    W = {};
    D.bubble = E.bubble = M.bubble = W.bubble = true;
    stepping = false;
    mispredicted = false;
    mem_sys.reset();
    input_buffer.clear();
    input_buffer.str("");

    elf_reader.load_elf(F.predPC, mem_sys);

    init_stack();

    running = true;
    tick = 0;
    instruction_count = 0;
    total_branch = correct_branch = 0;
    mispredicted_time = meet_jalr_time = data_dependent_time = 0;
    time_t begin_time = time(NULL);
    while (true) {
        f = {};
        d = {};
        e = {};
        m = {};
        w = {};

        int max_cycles = 0;
        const char *stage;
        try {
            stage = "WB";
            max_cycles = max(max_cycles, WB());
            stage = "MEM";
            max_cycles = max(max_cycles, MEM());
            stage = "EX";
            max_cycles = max(max_cycles, EX());
            stage = "ID";
            max_cycles = max(max_cycles, ID());
            stage = "IF";
            max_cycles = max(max_cycles, IF());
            stage = "ecall";
            if (W.opcode == OP_ECALL)
                max_cycles = max(max_cycles, process_syscall());
        } catch (const ExitEvent& e) {
            time_t total_time = time(NULL) - begin_time;
            printf("======== above are user output ========\n");
            printf("program exited %lu in %ld seconds\n", e.status, total_time);
            printf("instructions=%lu cycles=%lu CPI=%.3f\n", instruction_count,
                tick, (double)tick / instruction_count);
            printf("branch (%s): total_branch=%lu accuracy=%.3f%%\n", br_pred->get_name(),
                total_branch, (double)correct_branch / total_branch * 100);
            printf("mispredicted_time=%lu\n", mispredicted_time);
            printf("meet_jalr_time=%lu\n", meet_jalr_time);
            printf("data_dependent_time=%lu\n", data_dependent_time);
            mem_sys.print_info();
            printf("\n");
            break;
        } catch (const runtime_error& err) {
            printf("======== above are user output ========\n");
            printf("runtime_error in %s: %s\n", stage, err.what());
            print_pipeline();
            print_regs();
            mem_sys.print_info();
            printf("\n");
            break;
        }

        if (!single_step && verbose) {
            print_pipeline();
            print_regs();
        }

        if (single_step && check_breakpoint(E.pc)) {
            print_pipeline();
            if (process_command() == CMD_KILL)
                break;
        }

        process_control_signal();

        if (E.opcode == OP_BRANCH) {
            total_branch++;
            correct_branch += !mispredicted;
        }
        instruction_count += !W.stall && !W.bubble;

        tick += max_cycles;
        F.update(f);
        D.update(d);
        E.update(e);
        M.update(m);
        W.update(w);
    }
    running = false;
}

int Simulator::process_syscall()
{
    reg_t a1 = reg[REG_A1];
    switch (reg[REG_A7]) {
    case SYS_exit:
        throw ExitEvent(reg[REG_A0]);
    case SYS_cputchar:
        printf("%c", (char)a1);
        break;
    case SYS_sbrk:
        reg[REG_A0] = mem_sys.sbrk((size_t)a1);
        break;
    case SYS_readint: {
        int tmp;
        if (!(input_buffer >> tmp)) {
            input_buffer.clear();
            string buf;
            getline(cin, buf);
            input_buffer << buf;
            input_buffer >> tmp;
        }
        reg[REG_A0] = tmp;
        break;
    }
    case SYS_time:
        reg[REG_A0] = time(NULL);
        break;
    default:
        throw_error("unsupported syscall number %d", reg[REG_A7]);
    }
    return ecall_cycles[reg[REG_A7]];
}

void SIGINT_handler(int signum)
{
    siglongjmp(saved_env, 1);
}

void Simulator::start()
{
    if (!single_step) {
        run_prog();
        return;
    }
    printf("\nWelcome to the debugger of Jahoo's RISC-V simulator!\n\n");
    printf("It currently support gdb's command including 'run', 'kill'\n");
    printf("'set', 'breakpoint', 'continue', 'step', 'next',\n");
    printf("'info', 'print', 'x/' and 'quit'.\n\n");
    printf("Notice: the program is NOT loaded until the first run.\n\n");
    while (true) {
        if (process_command() == CMD_RUN) {
            auto old_handler = signal(SIGINT, SIGINT_handler);
            if (sigsetjmp(saved_env, true) == 0) {
                run_prog();
            } else {
                running = false;
                printf("======== above are user output ========\n");
                printf("received Ctrl-C, aborted\n");
                print_pipeline();
                print_regs();
                mem_sys.print_info();
            }
            signal(SIGINT, old_handler);
        }
    }
}
