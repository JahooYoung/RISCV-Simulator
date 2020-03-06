#include <stdlib.h>
#include <string.h>
#include "read_elf.h"

FILE *elf = NULL;
Elf64_Ehdr elf64_hdr;

//Program headers
unsigned int padr = 0;
unsigned int psize = 0;
unsigned int pnum = 0;

//Section Headers
unsigned int sadr = 0;
unsigned int ssize = 0;
unsigned int snum = 0;

//Symbol table
unsigned long long symnum = 0;
unsigned long long symadr = 0;
unsigned long long symsize = 0;

// unsigned int index = 0;

unsigned long long strtab_offset = 0;
unsigned long long strtab_size = 0;

bool open_file(char *filename)
{
    if (!(file = fopen(filename, "rb"))) {
        fprintf(stderr, "error: cannot open %s", filename);
        return false;
    }
    char info_filename[50];
    sprintf(info_filename, "%s-info.txt", filename);
    elf = fopen(info_filename, "w");

    return true;
}

void read_elf(char *filename)
{
    if (!open_file(filename))
        return;

    fprintf(elf, "ELF Header:\n");
    read_elf_header();

    fprintf(elf, "\n\nSection Headers:\n");
    read_section_headers();

    fprintf(elf, "\n\nProgram Headers:\n");
    read_program_headers();

    fprintf(elf, "\n\nSymbol table:\n");
    read_symtable();

    fclose(elf);
}

void read_elf_header()
{
    //file should be relocated
    fread(&elf64_hdr, sizeof(elf64_hdr), 1, file);

    fprintf(elf, "  magic number: %.4s\n", elf64_hdr.e_ident);
    fprintf(elf, "  Class: %u-bit\n", elf64_hdr.e_ident[EI_CLASS] * 32);
    fprintf(elf, "  Data: %s-endian\n", elf64_hdr.e_ident[EI_DATA] == 1 ? "little"  : "big");
    fprintf(elf, "  Version: %u\n", (int)elf64_hdr.e_ident[EI_VERSION]);
    fprintf(elf, "  OS/ABI:	%u\n", (int)elf64_hdr.e_ident[EI_OSABI]);
    fprintf(elf, "  ABI Version: %u\n", (int)elf64_hdr.e_ident[EI_ABIVERSION]);

    fprintf(elf, "  Type: %u\n", elf64_hdr.e_type);
    fprintf(elf, "  Machine:   \n");
    fprintf(elf, "  Version:  \n");

    fprintf(elf, "  Entry point address:  0x%lx\n", elf64_hdr.e_entry);

    fprintf(elf, "  Start of program headers: %lu bytes into file\n", elf64_hdr.e_phoff);
    fprintf(elf, "  Start of section headers: %lu bytes into file\n", elf64_hdr.e_shoff);
    fprintf(elf, "  Flags:  0x%x\n", elf64_hdr.e_flags);
    fprintf(elf, "  Size of this header: %u Bytes\n", elf64_hdr.e_ehsize);
    fprintf(elf, "  Size of program headers: %u Bytes\n", elf64_hdr.e_phentsize);
    fprintf(elf, "  Number of program headers: %u \n", elf64_hdr.e_phnum);
    fprintf(elf, "  Size of section headers: %u Bytes\n", elf64_hdr.e_shentsize);
    fprintf(elf, "  Number of section headers: %u\n", elf64_hdr.e_shnum);
    fprintf(elf, "  Section header string table index: %u\n", elf64_hdr.e_shstrndx);
}

void read_section_headers()
{
    Elf64_Shdr elf64_shdr;

    fseek(file, elf64_hdr.e_shoff + elf64_hdr.e_shstrndx * sizeof(Elf64_Shdr), SEEK_SET);
    fread(&elf64_shdr, sizeof(elf64_shdr), 1, file);
    char shstr[elf64_shdr.sh_size];
    fseek(file, elf64_shdr.sh_offset, SEEK_SET);
    fread(shstr, elf64_shdr.sh_size, 1, file);

    fseek(file, elf64_hdr.e_shoff, SEEK_SET);
    for (int i = 0; i < elf64_hdr.e_shnum; i++) {
        fread(&elf64_shdr, sizeof(elf64_shdr), 1, file);

        fprintf(elf, "  [%3d]\n", i);
        fprintf(elf, "  Name: %s\n", shstr + elf64_shdr.sh_name);
        fprintf(elf, "  Type: %u\n", elf64_shdr.sh_type);
        fprintf(elf, "  Address: %lx\n", elf64_shdr.sh_addr);
        fprintf(elf, "  Offest: %lx\n", elf64_shdr.sh_offset);
        fprintf(elf, "  Size: %lu\n", elf64_shdr.sh_size);
        fprintf(elf, "  Entsize: %lu\n", elf64_shdr.sh_entsize);
        fprintf(elf, "  Flags: %lx\n", elf64_shdr.sh_flags);
        fprintf(elf, "  Link: %u\n", elf64_shdr.sh_link);
        fprintf(elf, "  Info: %u\n", elf64_shdr.sh_info);
        fprintf(elf, "  Align: %lx\n", elf64_shdr.sh_addralign);

        if (elf64_shdr.sh_type == SHT_SYMTAB) {
            symadr = elf64_shdr.sh_offset;
            symsize = elf64_shdr.sh_size;
            symnum = elf64_shdr.sh_size / elf64_shdr.sh_entsize;
        }
        if (elf64_shdr.sh_type == SHT_STRTAB
            && strcmp(shstr + elf64_shdr.sh_name, ".strtab") == 0) {
            strtab_offset = elf64_shdr.sh_offset;
            strtab_size = elf64_shdr.sh_size;
        }
    }
}

void read_program_headers()
{
    Elf64_Phdr elf64_phdr;

    fseek(file, elf64_hdr.e_phoff, SEEK_SET);
    for (int i = 0; i < elf64_hdr.e_phnum; i++) {
        fread(&elf64_phdr, sizeof(elf64_phdr), 1, file);

        fprintf(elf, "  [%3d]\n", i);
        fprintf(elf, "  Type: %u\n", elf64_phdr.p_type);
        fprintf(elf, "  Flags: %u\n", elf64_phdr.p_flags);
        fprintf(elf, "  Offset: %lx\n", elf64_phdr.p_offset);
        fprintf(elf, "  VirtAddr: %lx\n", elf64_phdr.p_vaddr);
        fprintf(elf, "  PhysAddr: %lx\n", elf64_phdr.p_paddr);
        fprintf(elf, "  FileSiz: %lu\n", elf64_phdr.p_filesz);
        fprintf(elf, "  MemSiz: %lu\n", elf64_phdr.p_memsz);
        fprintf(elf, "  Align: %lu\n", elf64_phdr.p_align);
    }
}

void read_symtable()
{
    Elf64_Sym elf64_sym;

    char *stradr = (char*)malloc(strtab_size);
    fseek(file, strtab_offset, SEEK_SET);
    fread(stradr, strtab_size, 1, file);

    fseek(file, symadr, SEEK_SET);
    for (int i = 0; i < symnum; i++) {
        fread(&elf64_sym, sizeof(elf64_sym), 1, file);

        fprintf(elf, "  [%3d]\n", i);
        fprintf(elf, "  Name: %s\n", stradr + elf64_sym.st_name);
        fprintf(elf, "  Bind: %u\n", ELF64_ST_BIND(elf64_sym.st_info));
        fprintf(elf, "  Type: %u\n", ELF64_ST_TYPE(elf64_sym.st_info));
        fprintf(elf, "  NDX: %u\n", elf64_sym.st_shndx);
        fprintf(elf, "  Size: %lu\n", elf64_sym.st_size);
        fprintf(elf, "  Value: %lx\n", elf64_sym.st_value);
    }
    free(stradr);
}
