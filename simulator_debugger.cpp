#include <cstring>
#include <string>
#include <iostream>
#include "simulator.hpp"
#include "decode_helpers.hpp"
using namespace std;

const char *reg_abi_name[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

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

bool Simulator::check_breakpoint(reg_t pc)
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
