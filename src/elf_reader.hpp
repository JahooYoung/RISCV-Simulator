#ifndef ELF_READER_HPP
#define ELF_READER_HPP

#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include "types.hpp"
#include "memory_system.hpp"
#include "elf.hpp"

using InstructionMap = std::map<uintptr_t, std::string>;

using SymbolTable = std::map<std::string, Elf64_Sym>;

void fread_wrapper(void *ptr, size_t size, size_t nmemb, FILE *stream);

class ElfReader
{
private:
    std::string elf_filename;
    FILE *elf_file;

    Elf64_Ehdr elf64_hdr;
    std::vector<Elf64_Phdr> program_header;
    std::vector<Elf64_Shdr> section_header;
    char *shstr;

public:
    SymbolTable symtab;

    ElfReader(const std::string& elf_filename);
    ~ElfReader();
    void output_elf_info(const std::string& info_filename);
    void load_objdump(const std::string& objdump_path, InstructionMap& inst_map);
    void load_elf(reg_t& pc, MemorySystem& mem_sys);
};

#endif
