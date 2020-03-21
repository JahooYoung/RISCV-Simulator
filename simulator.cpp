#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include "simulator.hpp"
#include "decode_helpers.hpp"
using namespace std;
using nlohmann::json;

class ExitEvent {};

Simulator::Simulator(const json& config)
    : disasemble(config.value("disasemble", false)),
    single_step(config.value("single_step", false)),
    data_forwarding(config.value("data_forwarding", true)),
    verbose(config.value("verbose", false)),
    stack_size(config.value("stack_size", 1024)),  // KB
    elf_reader(config["elf_file"].get<string>())
{
    // read elf file
    string info_file = config.value("info_file", "");
    if (info_file != "")
        elf_reader.output_elf_info(info_file);
    if (disasemble)
        elf_reader.load_objdump(config.value("objdump", "riscv64-unknown-elf-objdump"), inst_map);

    // get branch predictor
    string bpred_str = config.value("branch_prediction", "btfnt");
    if (bpred_str == "never_taken")
        br_pred = new NeverTaken();
    else if (bpred_str == "always_taken")
        br_pred = new AlwaysTaken();
    else if (bpred_str == "btfnt")
        br_pred = new BTFNT();
    else {
        cerr << "error: no branch prediction strategy named " << bpred_str << endl;
        exit(EXIT_FAILURE);
    }

    // get alu cycles configuration
    json alu_json = config["alu_cycles"];
    alu_cycles[ALU_ADD] = alu_cycles[ALU_SUB] = alu_json.value("add_sub", 1);
    alu_cycles[ALU_MUL] = alu_cycles[ALU_MULH] = alu_cycles[ALU_MULHSU] =
        alu_cycles[ALU_MULHU] = alu_json.value("mul", 3);
    alu_cycles[ALU_DIV] = alu_cycles[ALU_DIVU] = alu_cycles[ALU_REM] =
        alu_cycles[ALU_REMU] = alu_json.value("div_rem", 30);
    alu_cycles[ALU_SLL] = alu_cycles[ALU_SRA] = alu_cycles[ALU_SRL] =
        alu_cycles[ALU_XOR] = alu_cycles[ALU_OR] = alu_cycles[ALU_AND] =
        alu_json.value("bit_op", 1);
    alu_cycles[ALU_SLT] = alu_cycles[ALU_SLTU] = alu_json.value("slt", 1);

    // get ecall cycles configuration
    json ecall_json = config["ecall_cycles"];
    ecall_cycles[SYS_cputchar] = ecall_json.value("cputchar", 1000);
    ecall_cycles[SYS_sbrk] = ecall_json.value("sbrk", 1000);
    ecall_cycles[SYS_readint] = ecall_json.value("readint", 10000);


}

Simulator::~Simulator()
{
    delete br_pred;
}

int Simulator::IF()
{
    // select pc
    reg_t pc = F.predPC;
    if (((M.opcode == OP_BRANCH && M.cond) || M.opcode == OP_JALR || M.opcode == OP_JAL)
        && M.predPC != M.valE)
        pc = M.valE;
    else if (M.opcode == OP_BRANCH && !M.cond && M.predPC != M.val2)
        pc = M.val2;

    int cycles = mem_sys.read_inst(pc, d.inst);
    if ((d.inst & 3) != 3)
        d.inst &= 0xFFFF;
    d.valP = pc;
    d.asm_str = inst_map[pc];

    // predict pc
    d.predPC = f.predPC = br_pred->predict(pc, d.inst);
    return cycles;
}

int Simulator::ID()
{
    if (D.bubble)
        return 0;

    e.asm_str = D.asm_str;
    e.valP = D.valP;
    e.predPC = D.predPC;
    // get opcode, funct3, imm, alu_op, rs1, rs2, rd, mem_op, compressed_inst
    if ((D.inst & 3) != 3) {
        e.compressed_inst = true;
        parse_16b_inst(D.inst, e);
    } else {
        parse_32b_inst(D.inst, e);
    }

    // get the register value of rs1 and rs2
    e.val1 = reg[e.rs1];
    e.val2 = reg[e.rs2];
    if (data_forwarding && e.rs1 != 0) {
        if (E.rd == e.rs1) {
            switch (E.opcode) {
            case OP_JALR:
            case OP_JAL:
            case OP_BRANCH:
                e.val1 = m.val2;
                break;
            default:
                e.val1 = m.valE;
            }
        } else if (M.rd == e.rs1) {
            e.val1 = w.val;
        } else if (W.rd == e.rs1) {
            e.val1 = W.val;
        }
    }
    if (data_forwarding && e.rs2 != 0) {
        if (E.rd == e.rs2) {
            switch (E.opcode) {
            case OP_JALR:
            case OP_JAL:
            case OP_BRANCH:
                e.val2 = m.val2;
                break;
            default:
                e.val2 = m.valE;
            }
        } else if (M.rd == e.rs2) {
            e.val2 = w.val;
        } else if (W.rd == e.rs2) {
            e.val2 = W.val;
        }
    }

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
    m.predPC = E.predPC;

    switch (E.opcode) {
    case OP_JALR:  // jalr
    case OP_JAL:  // jal
    case OP_BRANCH:  // beq, ...
        m.val2 = E.valP + (E.compressed_inst ? 2 : 4);
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
        valA = E.valP;
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
        if (M.funct3 < 4)
            cycles = mem_sys.read_data(M.valE, w.val, 1 << M.funct3);
        else
            cycles = mem_sys.read_data(M.valE, w.val, 1 << (M.funct3 - 4));
        switch (M.funct3) {
        case 0:
        case 1:
        case 2:
            w.val = sign_extend(w.val, 8 << M.funct3);
            break;
        case 4:  // lbu
        case 5:  // lhu
        case 6:  // lwu
            w.val = zero_extend(w.val, 8 << (M.funct3 - 4));
            break;
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

    mem_sys = MemorySystem();
    elf_reader.load_elf(F.predPC, mem_sys);

    // initialize stack
    reg[REG_SP] = 0xFFFFFFFFFFF0;
    for (int i = 0; i < ((stack_size + 3) / 4); i++)
        mem_sys.page_alloc(reg[REG_SP] - PGSIZE * i);

    running = true;
    tick = 0;
    instruction_count = 0;
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
            if (W.opcode == OP_ECALL) {
                max_cycles = max(max_cycles, process_syscall());
            }
        } catch (ExitEvent) {
            printf("======== above are user output ========\n");
            printf("program exited normally\n");
            printf("    ticks: %lu\n", tick);
            printf("    instructions: %lu\n", instruction_count);
            mem_sys.print_info();
            break;
        } catch (runtime_error err) {
            printf("======== above are user output ========\n");
            printf("runtime_error in %s: %s\n", stage, err.what());
            print_pipeline();
            print_regs();
            mem_sys.print_info();
            break;
        }

        if (!single_step && verbose) {
            print_pipeline();
            print_regs();
        }

        if (single_step && check_breakpoint(E.valP)) {
            print_pipeline();
            if (process_command() == 1)
                break;
        }

        // control logic: stall and bubble
        bool meet_ecall = (E.opcode == OP_ECALL || M.opcode == OP_ECALL || W.opcode == OP_ECALL);
        bool mispredicted =
            (((E.opcode == OP_BRANCH && m.cond) || E.opcode == OP_JALR || E.opcode == OP_JAL)
                && E.predPC != m.valE)
            || (E.opcode == OP_BRANCH && !m.cond && E.predPC != m.val2);

        d.bubble |= mispredicted;
        e.bubble |= mispredicted;

        bool data_dependant;
        if (data_forwarding) {
            data_dependant = (e.rs1 != 0 && e.rs1 == E.rd && E.opcode == OP_LOAD) ||
                             (e.rs2 != 0 && e.rs2 == E.rd && E.opcode == OP_LOAD);
        } else {
            data_dependant = (e.rs1 != 0 && (E.rd == e.rs1 || M.rd == e.rs1 || W.rd == e.rs1)) ||
                             (e.rs2 != 0 && (E.rd == e.rs2 || M.rd == e.rs2 || W.rd == e.rs2));
        }
        F.stall |= data_dependant | meet_ecall;
        D.stall |= data_dependant | meet_ecall;
        e.bubble |= data_dependant | meet_ecall;

        e.bubble |= D.bubble;
        m.bubble |= E.bubble;
        w.bubble |= M.bubble;

        if (!W.stall && !W.bubble)
            instruction_count++;

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
    static stringstream line;
    reg_t a1 = reg[REG_A1], a2 = reg[REG_A2];
    switch (reg[REG_A7]) {
    case SYS_exit:
        throw ExitEvent();
    case SYS_cputchar:
        printf("%c", (char)a1);
        break;
    case SYS_sbrk:
        reg[REG_A0] = mem_sys.sbrk((size_t)a1);
        break;
    case SYS_readint: {
        int tmp;
        if (!(line >> tmp)) {
            line.clear();
            string buf;
            getline(cin, buf);
            line << buf;
            line >> tmp;
        }
        reg[REG_A0] = tmp;
        break;
    }
    default:
        throw_error("unsupported syscall number %d", reg[REG_A7]);
    }
    return 0;
}

void Simulator::start()
{
    if (!single_step) {
        run_prog();
        return;
    }
    running = false;
    while (true) {
        if (process_command() == 2)
            run_prog();
    }
}
