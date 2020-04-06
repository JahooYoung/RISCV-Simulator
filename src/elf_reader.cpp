#include <iostream>
#include <cstring>
#include <cctype>
#include "elf_reader.hpp"
using namespace std;

void fread_wrapper(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size *= nmemb;
    size_t cnt = 0;
    while ((cnt = fread(ptr, 1, size, stream)) != size) {
        if (feof(stream) || ferror(stream))
            throw_error("cannot read elf file");
        ptr = ptr + cnt;
        size -= cnt;
    }
}

ElfReader::ElfReader(const string& _elf_filename)
    : elf_filename(_elf_filename)
{
    elf_file = fopen(elf_filename.c_str(), "rb");
    if (!elf_file) {
        cerr << "error: cannot open " << elf_filename << endl;
        exit(EXIT_FAILURE);
    }

    try {
        fread_wrapper(&elf64_hdr, sizeof(elf64_hdr), 1, elf_file);

        // read section header strings
        Elf64_Shdr elf64_shdr;
        fseek(elf_file, elf64_hdr.e_shoff + elf64_hdr.e_shstrndx * sizeof(Elf64_Shdr), SEEK_SET);
        fread_wrapper(&elf64_shdr, sizeof(elf64_shdr), 1, elf_file);
        shstr = new char[elf64_shdr.sh_size];
        fseek(elf_file, elf64_shdr.sh_offset, SEEK_SET);
        fread_wrapper(shstr, elf64_shdr.sh_size, 1, elf_file);

        // read section headers
        int symtab_addr = 0, symtab_num = 0;
        int strtab_offset = 0, strtab_size = 0;
        section_header.resize(elf64_hdr.e_shnum);
        fseek(elf_file, elf64_hdr.e_shoff, SEEK_SET);
        fread_wrapper(section_header.data(), sizeof(Elf64_Shdr), elf64_hdr.e_shnum, elf_file);
        for (const auto& elf64_shdr: section_header) {
            if (elf64_shdr.sh_type == SHT_SYMTAB) {
                symtab_addr = elf64_shdr.sh_offset;
                symtab_num = elf64_shdr.sh_size / elf64_shdr.sh_entsize;
            }
            if (elf64_shdr.sh_type == SHT_STRTAB
                && strcmp(shstr + elf64_shdr.sh_name, ".strtab") == 0) {
                strtab_offset = elf64_shdr.sh_offset;
                strtab_size = elf64_shdr.sh_size;
            }
        }

        // read program headers
        program_header.resize(elf64_hdr.e_phnum);
        fseek(elf_file, elf64_hdr.e_phoff, SEEK_SET);
        fread_wrapper(program_header.data(), sizeof(Elf64_Phdr), elf64_hdr.e_phnum, elf_file);

        // read symbol table
        char *stradr = new char[strtab_size];
        fseek(elf_file, strtab_offset, SEEK_SET);
        fread_wrapper(stradr, strtab_size, 1, elf_file);
        Elf64_Sym elf64_sym;
        fseek(elf_file, symtab_addr, SEEK_SET);
        for (int i = 0; i < symtab_num; i++) {
            fread_wrapper(&elf64_sym, sizeof(elf64_sym), 1, elf_file);
            symtab[stradr + elf64_sym.st_name] = elf64_sym;
        }
        delete[] stradr;
    } catch (runtime_error err) {
        cerr << "error: " << err.what() << endl;
        exit(EXIT_FAILURE);
    }
}

ElfReader::~ElfReader()
{
    delete[] shstr;
    fclose(elf_file);
}

void ElfReader::output_elf_info(const string& info_filename)
{
    FILE *info_file = fopen(info_filename.c_str(), "w");
    if (!info_file) {
        cerr << "error: cannot open " << info_filename << endl;
        return;
    }

    // print elf header
    fprintf(info_file, "ELF Header:\n");
    fprintf(info_file, "    magic number: %.4s\n", elf64_hdr.e_ident);
    fprintf(info_file, "    Class: %u-bit\n", elf64_hdr.e_ident[EI_CLASS] * 32);
    fprintf(info_file, "    Data: %s-endian\n", elf64_hdr.e_ident[EI_DATA] == 1 ? "little"  : "big");
    fprintf(info_file, "    Version: %u\n", (int)elf64_hdr.e_ident[EI_VERSION]);
    fprintf(info_file, "    OS/ABI:	%u\n", (int)elf64_hdr.e_ident[EI_OSABI]);
    fprintf(info_file, "    ABI Version: %u\n", (int)elf64_hdr.e_ident[EI_ABIVERSION]);

    fprintf(info_file, "    Type: %u\n", elf64_hdr.e_type);
    fprintf(info_file, "    Machine:  \n");
    fprintf(info_file, "    Version:  \n");

    fprintf(info_file, "    Entry point address:  0x%lx\n", elf64_hdr.e_entry);

    fprintf(info_file, "    Start of program headers: %lu bytes into file\n", elf64_hdr.e_phoff);
    fprintf(info_file, "    Start of section headers: %lu bytes into file\n", elf64_hdr.e_shoff);
    fprintf(info_file, "    Flags:  0x%x\n", elf64_hdr.e_flags);
    fprintf(info_file, "    Size of this header: %u Bytes\n", elf64_hdr.e_ehsize);
    fprintf(info_file, "    Size of program headers: %u Bytes\n", elf64_hdr.e_phentsize);
    fprintf(info_file, "    Number of program headers: %u \n", elf64_hdr.e_phnum);
    fprintf(info_file, "    Size of section headers: %u Bytes\n", elf64_hdr.e_shentsize);
    fprintf(info_file, "    Number of section headers: %u\n", elf64_hdr.e_shnum);
    fprintf(info_file, "    Section header string table index: %u\n", elf64_hdr.e_shstrndx);

    // print section headers
    fprintf(info_file, "\n\nSection Headers:\n");
    for (unsigned i = 0; i < section_header.size(); i++) {
        fprintf(info_file, "    [%2d]\n", i);
        fprintf(info_file, "        Name: %s\n", shstr + section_header[i].sh_name);
        fprintf(info_file, "        Type: %u\n", section_header[i].sh_type);
        fprintf(info_file, "        Address: %lx\n", section_header[i].sh_addr);
        fprintf(info_file, "        Offest: %lx\n", section_header[i].sh_offset);
        fprintf(info_file, "        Size: %lu\n", section_header[i].sh_size);
        fprintf(info_file, "        Entsize: %lu\n", section_header[i].sh_entsize);
        fprintf(info_file, "        Flags: %lx\n", section_header[i].sh_flags);
        fprintf(info_file, "        Link: %u\n", section_header[i].sh_link);
        fprintf(info_file, "        Info: %u\n", section_header[i].sh_info);
        fprintf(info_file, "        Align: %lx\n", section_header[i].sh_addralign);
    }

    // print program headers
    fprintf(info_file, "\n\nProgram Headers:\n");
    for (unsigned i = 0; i < program_header.size(); i++) {
        fprintf(info_file, "    [%3d]\n", i);
        fprintf(info_file, "        Type: %u\n", program_header[i].p_type);
        fprintf(info_file, "        Flags: %u\n", program_header[i].p_flags);
        fprintf(info_file, "        Offset: %lx\n", program_header[i].p_offset);
        fprintf(info_file, "        VirtAddr: %lx\n", program_header[i].p_vaddr);
        fprintf(info_file, "        PhysAddr: %lx\n", program_header[i].p_paddr);
        fprintf(info_file, "        FileSiz: %lu\n", program_header[i].p_filesz);
        fprintf(info_file, "        MemSiz: %lu\n", program_header[i].p_memsz);
        fprintf(info_file, "        Align: %lu\n", program_header[i].p_align);
    }

    // print symbol table
    fprintf(info_file, "\n\nSymbol table:\n");
    int i = 0;
    for (const auto& symbol: symtab) {
        fprintf(info_file, "    [%3d]\n", i++);
        fprintf(info_file, "        Name: %s\n", symbol.first.c_str());
        fprintf(info_file, "        Bind: %u\n", ELF64_ST_BIND(symbol.second.st_info));
        fprintf(info_file, "        Type: %u\n", ELF64_ST_TYPE(symbol.second.st_info));
        fprintf(info_file, "        NDX: %u\n", symbol.second.st_shndx);
        fprintf(info_file, "        Size: %lu\n", symbol.second.st_size);
        fprintf(info_file, "        Value: %lx\n", symbol.second.st_value);
    }

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
    try {
        for (const auto& elf64_phdr: program_header) {
            mem_sys.load_segment(elf_file, elf64_phdr);
        }
    } catch (runtime_error err) {
        cerr << "error: " << err.what() << endl;
        exit(EXIT_FAILURE);
    }
    pc = elf64_hdr.e_entry;
}
