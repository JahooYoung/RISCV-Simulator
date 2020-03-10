#include <cstring>
#include <string>
#include <stdexcept>
#include "simulator.hpp"
#include "decode_helpers.hpp"
using namespace std;
using nlohmann::json;

Simulator::Simulator(const json& config)
    : elf_reader(config["elf_file"].get<string>()),
    single_step(config.value("single_step", false))
{
    auto info_file = config.value("info_file", "");
    if (info_file != "") {
        elf_reader.output_elf_info(info_file);
    }
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
    mem_sys.read_inst(pc, d.inst);

    // increase pc
    if ((d.inst & 3) != 3)
        pc += 2;
    else
        pc += 4;
    d.valP = pc;

    // predict pc
    f.predPC = br_pred->predict(pc, d.inst);

    // printf("pc: %llx\n", pc);
    // printf("inst: %x\n", inst);
}

template<class... Args>
void throw_error(const char *format, Args... args)
{
    char msg[100];
    sprintf(msg, format, args...);
    throw runtime_error(msg);
}

void Simulator::ID()
{
    if (D.bubble) {
        e.bubble = true;
        return;
    }
    memset(&e, 0, sizeof(e));
    if ((D.inst & 3) != 3) {
        // compressed instructions
    } else {
        uint8_t opcode = (uint8_t)getbits(D.inst, 0, 7);
        uint8_t funct7 = 0;
        char msg_template[] = "unknown instruction: opcode 0x%2x funct3 0x%2x funct7 0x%2x";
        switch (opcode) {
        case 0x33: // R-TYPE
            parse_R_Type(D.inst, e, funct7);
            try {
                e.alu_op = R_alu_op_map.at(e.funct3).at(funct7);
            } catch (out_of_range err) {
                throw_error(msg_template, opcode, e.funct3, funct7);
            }
            // switch (e.funct3) {
            // case 0x0:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_ADD; break;
            //     case 0x01: e.alu_op = ALU_MUL; break;
            //     case 0x20: e.alu_op = ALU_SUB; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // case 0x1:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_SLL; break;
            //     case 0x01: e.alu_op = ALU_MULH; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // case 0x2:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_SLT; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // case 0x4:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_XOR; break;
            //     case 0x01: e.alu_op = ALU_DIV; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // case 0x5:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_SRL; break;
            //     case 0x20: e.alu_op = ALU_SRA; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // case 0x6:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_OR; break;
            //     case 0x01: e.alu_op = ALU_REM; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // case 0x7:
            //     switch (funct7) {
            //     case 0x00: e.alu_op = ALU_AND; break;
            //     default:
            //         throw_error(msg_template, opcode, e.funct3, funct7);
            //     }
            //     break;
            // default:
            //     throw_error(msg_template, opcode, e.funct3, funct7);
            // }
            break;


        default:
            throw_error(msg_template, opcode, 0, 0);
        }
        // select val1, val2
        e.val1 = reg[e.rs1];
        e.val2 = reg[e.rs2];
    }
}

void Simulator::run()
{
    elf_reader.load_elf(F.predPC, mem_sys);
    memset(reg, 0, sizeof(reg));

    int tick = 0;
    while (true) {
        IF();
        ID();
        EX();
        MEM();
        WB();

        tick++;
        // TODO: add control logic
        F = f;
        D = d;
        E = e;
        M = m;
        W = w;
    }
}
