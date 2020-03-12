#include <iostream>
#include <cstring>
#include <cctype>
#include "elf_reader.hpp"
using namespace std;

ElfReader::ElfReader(const string& _elf_filename)
    : elf_filename(_elf_filename)
{
    elf_file = fopen(elf_filename.c_str(), "rb");
    if (!elf_file) {
        cerr << "error: cannot open " << elf_filename << endl;
        exit(EXIT_FAILURE);
    }
    fread(&elf64_hdr, sizeof(elf64_hdr), 1, elf_file);
}

ElfReader::~ElfReader()
{
    fclose(elf_file);
}

void ElfReader::read_elf_header()
{
    fprintf(info_file, "  magic number: %.4s\n", elf64_hdr.e_ident);
    fprintf(info_file, "  Class: %u-bit\n", elf64_hdr.e_ident[EI_CLASS] * 32);
    fprintf(info_file, "  Data: %s-endian\n", elf64_hdr.e_ident[EI_DATA] == 1 ? "little"  : "big");
    fprintf(info_file, "  Version: %u\n", (int)elf64_hdr.e_ident[EI_VERSION]);
    fprintf(info_file, "  OS/ABI:	%u\n", (int)elf64_hdr.e_ident[EI_OSABI]);
    fprintf(info_file, "  ABI Version: %u\n", (int)elf64_hdr.e_ident[EI_ABIVERSION]);

    fprintf(info_file, "  Type: %u\n", elf64_hdr.e_type);
    fprintf(info_file, "  Machine:   \n");
    fprintf(info_file, "  Version:  \n");

    fprintf(info_file, "  Entry point address:  0x%lx\n", elf64_hdr.e_entry);

    fprintf(info_file, "  Start of program headers: %lu bytes into file\n", elf64_hdr.e_phoff);
    fprintf(info_file, "  Start of section headers: %lu bytes into file\n", elf64_hdr.e_shoff);
    fprintf(info_file, "  Flags:  0x%x\n", elf64_hdr.e_flags);
    fprintf(info_file, "  Size of this header: %u Bytes\n", elf64_hdr.e_ehsize);
    fprintf(info_file, "  Size of program headers: %u Bytes\n", elf64_hdr.e_phentsize);
    fprintf(info_file, "  Number of program headers: %u \n", elf64_hdr.e_phnum);
    fprintf(info_file, "  Size of section headers: %u Bytes\n", elf64_hdr.e_shentsize);
    fprintf(info_file, "  Number of section headers: %u\n", elf64_hdr.e_shnum);
    fprintf(info_file, "  Section header string table index: %u\n", elf64_hdr.e_shstrndx);
}

void ElfReader::read_section_headers()
{
    Elf64_Shdr elf64_shdr;

    fseek(elf_file, elf64_hdr.e_shoff + elf64_hdr.e_shstrndx * sizeof(Elf64_Shdr), SEEK_SET);
    fread(&elf64_shdr, sizeof(elf64_shdr), 1, elf_file);
    char shstr[elf64_shdr.sh_size];
    fseek(elf_file, elf64_shdr.sh_offset, SEEK_SET);
    fread(shstr, elf64_shdr.sh_size, 1, elf_file);

    fseek(elf_file, elf64_hdr.e_shoff, SEEK_SET);
    for (int i = 0; i < elf64_hdr.e_shnum; i++) {
        fread(&elf64_shdr, sizeof(elf64_shdr), 1, elf_file);

        fprintf(info_file, "  [%3d]\n", i);
        fprintf(info_file, "  Name: %s\n", shstr + elf64_shdr.sh_name);
        fprintf(info_file, "  Type: %u\n", elf64_shdr.sh_type);
        fprintf(info_file, "  Address: %lx\n", elf64_shdr.sh_addr);
        fprintf(info_file, "  Offest: %lx\n", elf64_shdr.sh_offset);
        fprintf(info_file, "  Size: %lu\n", elf64_shdr.sh_size);
        fprintf(info_file, "  Entsize: %lu\n", elf64_shdr.sh_entsize);
        fprintf(info_file, "  Flags: %lx\n", elf64_shdr.sh_flags);
        fprintf(info_file, "  Link: %u\n", elf64_shdr.sh_link);
        fprintf(info_file, "  Info: %u\n", elf64_shdr.sh_info);
        fprintf(info_file, "  Align: %lx\n", elf64_shdr.sh_addralign);

        if (elf64_shdr.sh_type == SHT_SYMTAB) {
            symtab_addr = elf64_shdr.sh_offset;
            symtab_size = elf64_shdr.sh_size;
            symtab_num = elf64_shdr.sh_size / elf64_shdr.sh_entsize;
        }
        if (elf64_shdr.sh_type == SHT_STRTAB
            && strcmp(shstr + elf64_shdr.sh_name, ".strtab") == 0) {
            strtab_offset = elf64_shdr.sh_offset;
            strtab_size = elf64_shdr.sh_size;
        }
    }
}

void ElfReader::read_program_headers()
{
    Elf64_Phdr elf64_phdr;

    fseek(elf_file, elf64_hdr.e_phoff, SEEK_SET);
    for (int i = 0; i < elf64_hdr.e_phnum; i++) {
        fread(&elf64_phdr, sizeof(elf64_phdr), 1, elf_file);

        fprintf(info_file, "  [%3d]\n", i);
        fprintf(info_file, "  Type: %u\n", elf64_phdr.p_type);
        fprintf(info_file, "  Flags: %u\n", elf64_phdr.p_flags);
        fprintf(info_file, "  Offset: %lx\n", elf64_phdr.p_offset);
        fprintf(info_file, "  VirtAddr: %lx\n", elf64_phdr.p_vaddr);
        fprintf(info_file, "  PhysAddr: %lx\n", elf64_phdr.p_paddr);
        fprintf(info_file, "  FileSiz: %lu\n", elf64_phdr.p_filesz);
        fprintf(info_file, "  MemSiz: %lu\n", elf64_phdr.p_memsz);
        fprintf(info_file, "  Align: %lu\n", elf64_phdr.p_align);
    }
}

void ElfReader::read_symtable()
{
    Elf64_Sym elf64_sym;

    char *stradr = (char*)malloc(strtab_size);
    fseek(elf_file, strtab_offset, SEEK_SET);
    fread(stradr, strtab_size, 1, elf_file);

    fseek(elf_file, symtab_addr, SEEK_SET);
    for (int i = 0; i < symtab_num; i++) {
        fread(&elf64_sym, sizeof(elf64_sym), 1, elf_file);

        fprintf(info_file, "  [%3d]\n", i);
        fprintf(info_file, "  Name: %s\n", stradr + elf64_sym.st_name);
        fprintf(info_file, "  Bind: %u\n", ELF64_ST_BIND(elf64_sym.st_info));
        fprintf(info_file, "  Type: %u\n", ELF64_ST_TYPE(elf64_sym.st_info));
        fprintf(info_file, "  NDX: %u\n", elf64_sym.st_shndx);
        fprintf(info_file, "  Size: %lu\n", elf64_sym.st_size);
        fprintf(info_file, "  Value: %lx\n", elf64_sym.st_value);
    }
    free(stradr);
}

void ElfReader::output_elf_info(const string& info_filename)
{
    info_file = fopen(info_filename.c_str(), "w");
    if (!info_file) {
        cerr << "error: cannot open " << info_filename << endl;
        return;
    }

    fprintf(info_file, "ELF Header:\n");
    read_elf_header();

    fprintf(info_file, "\n\nSection Headers:\n");
    read_section_headers();

    fprintf(info_file, "\n\nProgram Headers:\n");
    read_program_headers();

    fprintf(info_file, "\n\nSymbol table:\n");
    read_symtable();

    fclose(info_file);
}

void ElfReader::load_objdump(const string& objdump_path, InstructionMap& inst_map)
{
    string cmd = objdump_path + " -d " + elf_filename;
    FILE *objdump_out = popen(cmd.c_str(), "r");

    size_t n = 100;
    char *line = new char[n];
    char buf[100];
    while (getline(&line, &n, objdump_out) > 0) {
        int len = strlen(line);
        if (len < 9 || line[8] != ':')
            continue;
        line[len - 1] = '\0';  // remove '\n'
        uintptr_t addr = strtol(line, NULL, 16);
        char *st = line + 20;
        while (!isalpha(*st))
            st++;
        sprintf(buf, "%5lx:   ", addr);
        inst_map[addr] = buf + string(st);
    }
    delete[] line;

    pclose(objdump_out);
}

void ElfReader::load_elf(reg_t& pc, MemorySystem& mem_sys)
{
    Elf64_Phdr elf64_phdr;

    for (int i = 0; i < elf64_hdr.e_phnum; i++) {
        fseek(elf_file, elf64_hdr.e_phoff + i * sizeof(elf64_phdr), SEEK_SET);
        fread(&elf64_phdr, sizeof(elf64_phdr), 1, elf_file);
        mem_sys.load_segment(elf_file, elf64_phdr);
    }
    pc = elf64_hdr.e_entry;
}
