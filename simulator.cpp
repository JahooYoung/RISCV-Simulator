#include <cstring>
#include <string>
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
    : elf_reader(config["elf_file"].get<string>()),
    disasemble(config.value("disasemble", false)),
    single_step(config.value("single_step", false)),
    data_forwarding(config.value("data_forwarding", false))
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
    // TODO: select pc
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

    // write asm_str
    // char buf[100];
    // if ((d.inst & 3) != 3) {
    //     printf("==> %5lx: %-8.4x\n", pc, (uint16_t)d.inst);
    // } else {
    //     printf("==> %5lx: %.8x\n", pc, d.inst);
    // }
    d.asm_str = inst_map[pc];

    // increase pc
    pc += (d.inst & 3) != 3 ? 2 : 4;

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
    case ALU_MULH: m.valE = ((__uint128_t)valA * valB) >> 64; break;
    case ALU_DIV: m.valE = valA / valB; break;
    case ALU_REM: m.valE = valA % valB; break;
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

void Simulator::run()
{
    memset(reg, 0, sizeof(reg));
    F = {};
    D = {};
    E = {};
    M = {};
    W = {};
    D.bubble = E.bubble = M.bubble = W.bubble = true;

    elf_reader.load_elf(F.predPC, mem_sys);
    // initialize stack pointer
    reg[2] = 0xFFFFFFFFFFF0;
    mem_sys.page_alloc(reg[2]);

    int tick = 0;
    while (tick < 3000) {
        f = {};
        d = {};
        e = {};
        m = {};
        w = {};

        try {
            // printf("IF.."); fflush(stdout);
            IF();
            // printf("ID.."); fflush(stdout);
            ID();
            // printf("EX.."); fflush(stdout);
            EX();
            // printf("MEM.."); fflush(stdout);
            MEM();
            // printf("WB.."); fflush(stdout);
            WB();
        } catch (runtime_error err) {
            printf("runtime_error: %s\n", err.what());
            break;
        }

        printf("\ntick = %d\n", tick);
        // printf("    IF: %s\n", inst_map[F.predPC].c_str());
        printf("    IF: "); d.print_inst();
        F.print();
        printf("    ID: "); D.print_inst();
        D.print();
        printf("    EX: "); E.print_inst();
        E.print();
        printf("    MM: "); M.print_inst();
        M.print();
        printf("    WB: "); W.print_inst();
        W.print();

        printf("    Registers:");
        for (int i = 0; i < 32; i++) {
            if (i % 8 == 0)
                printf("\n    ");
            printf("%4s=%-6lx", reg_abi_name[i], reg[i]);
        }
        printf("\n\n");

        // TODO: add control logic

        bool mispredicted =
            (((E.opcode == OP_BRANCH && m.cond) || E.opcode == OP_JALR || E.opcode == OP_JAL)
                && E.predPC != m.valE)
            || (E.opcode == OP_BRANCH && !m.cond && E.predPC != m.val2);

        d.bubble |= mispredicted;
        e.bubble |= mispredicted;

        if (!data_forwarding) {
            bool data_dep = (e.rs1 != 0 && (E.rd == e.rs1 || M.rd == e.rs1 || W.rd == e.rs1))
                        || (e.rs2 != 0 && (E.rd == e.rs2 || M.rd == e.rs2 || W.rd == e.rs2));
            F.stall |= data_dep;
            D.stall |= data_dep;
            e.bubble |= data_dep;
        }

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

    for (int i = 0; i < 10; i++) {
        uintptr_t va = 0x11528 + i * 4;
        reg_t value;
        mem_sys.read_data(va, value);
        printf("   value=%x\n", (uint32_t)value);
    }
}
