#ifndef ELF_READER_HPP
#define ELF_READER_HPP

#include <string>
#include <cstdio>
#include "types.hpp"
#include "memory_system.hpp"
#include "elf.hpp"

class ElfReader
{
private:
    FILE *elf_file;
    FILE *info_file;

    Elf64_Ehdr elf64_hdr;
    int symtab_addr, symtab_size, symtab_num;
    int strtab_offset, strtab_size;

public:
    ElfReader(const std::string& elf_filename);
    ~ElfReader();

    void read_elf_header();
    void read_section_headers();
    void read_program_headers();
    void read_symtable();
    void output_elf_info(const std::string& info_filename);

    void load_elf(reg_t& pc, MemorySystem& mem_sys);
};

#endif
