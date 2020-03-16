#include <cstring>
#include <string>
#include <iostream>
#include "simulator.hpp"
#include "decode_helpers.hpp"
using namespace std;
using nlohmann::json;

const char *reg_abi_name[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

Simulator::Simulator(const json& config)
    : disasemble(config.value("disasemble", false)),
    single_step(config.value("single_step", false)),
    data_forwarding(config.value("data_forwarding", true)),
    verbose(config.value("verbose", false)),
    elf_reader(config["elf_file"].get<string>())
{
    string info_file = config.value("info_file", "");
    if (info_file != "")
        elf_reader.output_elf_info(info_file);
    if (disasemble)
        elf_reader.load_objdump(config.value("objdump", "riscv64-unknown-elf-objdump"), inst_map);
    br_pred = new NeverTaken();
}

Simulator::~Simulator()
{
    delete br_pred;
}

void Simulator::IF()
{
    // select pc
    reg_t pc = F.predPC;
    if (((M.opcode == OP_BRANCH && M.cond) || M.opcode == OP_JALR || M.opcode == OP_JAL)
        && M.predPC != M.valE)
        pc = M.valE;
    else if (M.opcode == OP_BRANCH && !M.cond && M.predPC != M.val2)
        pc = M.val2;

    mem_sys.read_inst(pc, d.inst);
    if ((d.inst & 3) != 3)
        d.inst &= 0xFFFF;
    d.valP = pc;
    d.asm_str = inst_map[pc];

    // predict pc
    d.predPC = f.predPC = br_pred->predict(pc, d.inst);
}

void Simulator::ID()
{
    if (D.bubble)
        return;

    e.asm_str = D.asm_str;
    e.valP = D.valP;
    e.predPC = D.predPC;
    // get opcode, funct3, imm, alu_op, rs1, rs2, rd, mem_op, compressed_inst
    if ((D.inst & 3) != 3) {
        e.compressed_inst = true;
        parse_16b_inst(D, e);
    } else {
        parse_32b_inst(D, e);
    }

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
}

void Simulator::EX()
{
    if (E.bubble)
        return;

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
    case ALU_SLT: m.valE = valA < valB; break;
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
}

void Simulator::MEM()
{
    if (M.bubble)
        return;

    w.asm_str = M.asm_str;
    w.rd = M.rd;

    int remains = 0;
    switch (M.opcode) {
    case OP_LOAD:
        remains = mem_sys.read_data(M.valE, w.val);
        if (M.funct3 < 3)
            w.val = sign_extend(w.val, 8 << M.funct3);
        break;
    case OP_STORE:
        remains = mem_sys.write_data(M.valE, M.val2, 1 << M.funct3);
        break;
    case OP_JALR:  // jalr
    case OP_JAL:  // jal
    case OP_BRANCH:  // beq, ...
        w.val = M.val2;
        break;
    default:
        w.val = M.valE;
    }
}

void Simulator::WB()
{
    if (W.bubble)
        return;

    if (W.rd != 0)
        reg[W.rd] = W.val;
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
    stepping = single_step;

    mem_sys = MemorySystem();
    elf_reader.load_elf(F.predPC, mem_sys);
    // initialize stack pointer
    reg[REG_SP] = 0xFFFFFFFFFFF0;
    mem_sys.page_alloc(reg[REG_SP]);

    running = true;
    tick = 0;
    while (true) {
        f = {};
        d = {};
        e = {};
        m = {};
        w = {};

        try {
            // printf("WB.."); fflush(stdout);
            WB();
            // printf("MEM.."); fflush(stdout);
            MEM();
            // printf("EX.."); fflush(stdout);
            EX();
            // printf("ID.."); fflush(stdout);
            ID();
            // printf("IF.."); fflush(stdout);
            IF();
        } catch (runtime_error err) {
            printf("runtime_error: %s\n", err.what());
            print_pipeline();
            print_regs();
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
        F.stall |= data_dependant;
        D.stall |= data_dependant;
        e.bubble |= data_dependant;

        e.bubble |= D.bubble;
        m.bubble |= E.bubble;
        w.bubble |= M.bubble;

        tick++;
        F.update(f);
        D.update(d);
        E.update(e);
        M.update(m);
        W.update(w);
    }
    running = false;
    printf("program exited\n");
}

void Simulator::print_regs()
{
    printf("    Registers:");
    for (int i = 0; i < REG_NUM; i++) {
        if (i % 8 == 0)
            printf("\n    ");
        printf("%4s=%-6lx", reg_abi_name[i], reg[i]);
    }
    printf("\n\n");
}

void Simulator::print_pipeline()
{
    printf("\ntick = %lu\n", tick);
    // printf("    IF: %s\n", inst_map[F.predPC].c_str());
    printf("    IF: "); d.print_inst();  // `d` has the correct instruction
    F.print();
    printf("    ID: "); D.print_inst();
    D.print();
    printf("    EX: "); E.print_inst();
    E.print();
    printf("    MM: "); M.print_inst();
    M.print();
    printf("    WB: "); W.print_inst();
    W.print();
}

inline bool Simulator::check_breakpoint(reg_t pc)
{
    if (stepping) {
        stepping = false;
        return true;
    }
    return breakpoints.count(pc) > 0;
}

int Simulator::process_command()
{
    static string cmd, arg;
    while (true) {
        printf("(sim) "); fflush(stdout);
        string line;
        getline(cin, line);
        if (line != "") {
            size_t space = line.find_first_of(' ');
            if (space < line.size() - 1)
                arg = line.substr(space + 1);
            else
                arg = "";
            cmd = line.substr(0, space);
        }
        if (cmd == "q" || cmd == "quit") {
            exit(EXIT_SUCCESS);
        }
        if (cmd == "r" || cmd == "run") {
            if (running) {
                printf("error: the program is already running\n");
                continue;
            }
            return 2;
        }
        if (cmd == "c" || cmd == "continue") {
            stepping = false;
            break;
        }
        if (cmd == "b" || cmd == "break") {
            if (arg == "") {
                printf("error: no address/symbol provided\n");
                continue;
            }
            long long addr;
            try {
                addr = stoll(arg, nullptr, 0);
            } catch (invalid_argument) {
                try {
                    addr = elf_reader.symtab.at(arg).st_value;
                } catch (out_of_range err) {
                    printf("error: cannot find symbol %s\n", arg.c_str());
                    continue;
                }
            }
            breakpoints.insert(addr);
            printf("add breakpoint at 0x%llx\n", addr);
            continue;
        }
        if (cmd == "p" || cmd == "print") {
            if (arg == "") {
                printf("error: no register provided\n");
                continue;
            }
            if (arg[0] == '$') {
                string reg_name = arg.substr(1);
                if (reg_name == "") {
                    print_regs();
                    continue;
                }
                int index = 0;
                for (; index < 32 && reg_abi_name[index] != reg_name; index++);
                if (index == 32) {
                    printf("error: no register has name `%s`", reg_name.c_str());
                    continue;
                }
                printf("%s=0x%lx(%ld)\n", reg_name.c_str(), reg[index], reg[index]);
                continue;
            }
            printf("error: unsupported expression\n");
            continue;
        }
        if (cmd.size() >= 2 && cmd[0] == 'x' && cmd[1] == '/') {
            if (arg == "") {
                printf("error: no address/symbol provided\n");
                continue;
            }
            size_t size = 0;
            long long addr;
            try {
                addr = stoll(arg, nullptr, 0);
            } catch (invalid_argument) {
                try {
                    const auto& symbol = elf_reader.symtab.at(arg);
                    addr = symbol.st_value;
                    size = symbol.st_size;
                } catch (out_of_range err) {
                    printf("error: cannot find symbol %s\n", arg.c_str());
                    continue;
                }
            }
            string format = cmd.substr(2);
            size_t length, nxt = 0;
            try {
                length = stoll(format, &nxt);
                format = format.substr(nxt);
            } catch (invalid_argument) {}
            char fm = 'd', sz = 'g';
            size_t step_size = 8;
            for (char c: format) {
                switch (c) {
                case 'x':
                case 'd':
                case 'u':
                case 'f':
                case 'c':
                case 's':
                    fm = c;
                    break;
                case 'b':
                    step_size >>= 1;
                case 'h':
                    step_size >>= 1;
                case 'w':
                    step_size >>= 1;
                case 'g':
                    sz = c;
                    break;
                }
            }
            if (nxt == 0)
                length = max(1UL, size / step_size);
            mem_sys.output_memory(addr, fm, sz, length);
            continue;
        }
        // commands below require the program to be running
        if (cmd == "k" || cmd == "kill") {
            if (!running) {
                printf("error: program is not running\n");
                continue;
            }
            return 1;
        }
        if (cmd == "s" || cmd == "step") {
            if (!running) {
                printf("error: program is not running\n");
                continue;
            }
            stepping = true;
            break;
        }
        if (cmd == "n" || cmd == "next") {
            if (!running) {
                printf("error: program is not running\n");
                continue;
            }
            stepping = true;
            break;
        }
        printf("error: unknown command\n");
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
