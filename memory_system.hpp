#ifndef MEMORY_SYSTEM_HPP
#define MEMORY_SYSTEM_HPP

#include <cstdio>
#include <map>
#include "types.hpp"
#include "elf.hpp"

typedef uint64_t pte_t;

#define PGSIZE      4096

#define PGADDR(pte)     (pte & ~0xFFFULL)
#define PTE_ADDR(pte)   PGADDR(pte)
#define PGOFF(la)	    (((uintptr_t) (la)) & 0xFFF)

#define E_NO_MEM 1

#define HEAP_START 0x800000000LL

class MemorySystem
{
private:
    std::map<uintptr_t, pte_t> page_table;
    uintptr_t heap_pointer;

    uintptr_t translate(reg_t ptr);

public:
    MemorySystem();
    ~MemorySystem();
    pte_t page_alloc(uintptr_t va);
    void load_segment(FILE *file, const Elf64_Phdr& phdr);
    // return the number of cycles required
    int read_inst(reg_t ptr, inst_t& st);
    int read_data(reg_t ptr, reg_t& reg, int bytes);
    int write_data(reg_t ptr, reg_t reg, int bytes);
    uintptr_t sbrk(size_t size);
    void output_memory(uintptr_t va, char fm, char sz, size_t length);
    void print_info();
};

#endif
