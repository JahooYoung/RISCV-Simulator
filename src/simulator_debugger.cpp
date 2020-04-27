#include <string>
#include <iostream>
#include "simulator.hpp"
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

uint64_t Simulator::evaluate(const string& exp)
{
    if (exp[0] == '$') {
        string reg_name = exp.substr(1);
        int index = 0;
        for (; index < 32 && reg_abi_name[index] != reg_name; index++);
        if (index == 32) {
            throw_error("no register has name `%s`", reg_name.c_str());
        }
        return reg[index];
    }
    try {
        return stoull(exp, nullptr, 0);
    } catch (const invalid_argument&) {
        try {
            return elf_reader.symtab.at(exp).st_value;
        } catch (const out_of_range& err) {
            throw_error("cannot find symbol %s", exp.c_str());
        }
    }
}

inline bool is_prefix(const string& pre, const string& str)
{
    return str.find(pre) == 0;
}

void split(const string& s, ArgumentVector& result, char delim)
{
    result.clear();
    string::size_type last_pos = s.find_first_not_of(delim);
    string::size_type pos = s.find_first_of(delim, last_pos);
    while (last_pos != string::npos || pos != string::npos) {
        result.emplace_back(s.substr(last_pos, pos - last_pos));
        last_pos = s.find_first_not_of(delim, pos);
        pos = s.find_first_of(delim, last_pos);
    }
}

#define REQUIRE_RUNNING if (!running) throw_error("program is not running")

#define REQUIRE_NOT_RUNNING if (running) throw_error("the program is already running")

#define EXPECT(posi, content) \
    if (cmdline.size() <= posi) \
        throw_error("error: expect " content " at the %dnd argument\n", posi)

#define EXPECT_EXPRESSION(posi) EXPECT(posi, "expression")

Simulator::cmd_num_t Simulator::process_command()
{
    static ArgumentVector cmdline;
    while (true) {
        printf("(sim) "); fflush(stdout);
        string line;
        getline(cin, line);
        if (line != "")
            split(line, cmdline, ' ');
        if (cmdline.size() == 0)
            continue;

        try {
            const string& cmd = cmdline[0];
            if (is_prefix(cmd, "quit")) {
                exit(EXIT_SUCCESS);
            }
            else if (is_prefix(cmd, "run")) {
                REQUIRE_NOT_RUNNING;
                if (cmdline.size() > 1) {
                    argv.resize(1);
                    argv.insert(argv.end(), cmdline.begin() + 1, cmdline.end());
                }
                return CMD_RUN;
            }
            else if (cmd == "set") {
                if (cmdline.size() == 1) {
                    printf("error: argument required\n");
                    continue;
                }
                if (is_prefix(cmdline[1], "args")) {
                    argv.resize(1);
                    argv.insert(argv.end(), cmdline.begin() + 2, cmdline.end());
                } else {
                    printf("error: only 'set args' is supported currently\n");
                }
            }
            else if (is_prefix(cmd, "breakpoint")) {
                EXPECT_EXPRESSION(1);
                uintptr_t addr = evaluate(cmdline[1]);
                breakpoints.insert(addr);
                printf("added breakpoint at 0x%lx\n", addr);
            }
            else if (is_prefix(cmd, "print")) {
                EXPECT_EXPRESSION(1);
                uint64_t value = evaluate(cmdline[1]);
                printf("%s=0x%lx(%ld)\n", cmdline[1].c_str(), value, value);
            }
            else if (is_prefix(cmd, "info")) {
                if (cmdline.size() == 1) {
                    printf("\"info\" must be followed by the name of an info command.\n");
                    printf("List of info subcommands:\n\n");
                    printf("info registers -- List of registers and their contents\n");
                    printf("info breakpoints -- List all breakpoints\n\n");
                } else if (is_prefix(cmdline[1], "registers")) {
                    print_regs();
                } else if (is_prefix(cmdline[1], "breakpoints")) {
                    int index = 0;
                    for (auto bp: breakpoints)
                        printf("%d: %lx\n", index++, bp);
                } else {
                    throw_error("undefined info command");
                }
            }
            else if (cmd.size() >= 2 && cmd[0] == 'x' && cmd[1] == '/') {
                EXPECT_EXPRESSION(1);
                uintptr_t addr = evaluate(cmdline[1]);
                size_t size = 0;
                auto it = elf_reader.symtab.find(cmdline[1]);
                if (it != elf_reader.symtab.end())
                    size = it->second.st_size;
                string format = cmd.substr(2);
                size_t length = 0, nxt = 0;
                try {
                    length = stoull(format, &nxt);
                    format = format.substr(nxt);
                } catch (const invalid_argument&) {}
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
            }
            else if (is_prefix(cmd, "kill")) {
                REQUIRE_RUNNING;
                return CMD_KILL;
            }
            else if (is_prefix(cmd, "continue")) {
                REQUIRE_RUNNING;
                stepping = false;
                return CMD_CONTINUE;
            }
            else if (is_prefix(cmd, "step")) {
                REQUIRE_RUNNING;
                stepping = true;
                return CMD_CONTINUE;
            }
            else if (is_prefix(cmd, "next")) {
                REQUIRE_RUNNING;
                stepping = true;
                return CMD_CONTINUE;
            }
            else {
                throw_error("unknown command");
            }
        } catch (const runtime_error& err) {
            printf("error: %s\n", err.what());
        }
    }
}
