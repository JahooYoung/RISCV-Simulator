#ifndef MEMORY_SYSTEM_HPP
#define MEMORY_SYSTEM_HPP

#include <cstdio>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "types.hpp"
#include "elf.hpp"
#include "cache.hpp"

typedef uint64_t pte_t;

#define PGSIZE      (1U << 12)

#define PGADDR(pte)     (pte & ~0xFFFULL)
#define PTE_ADDR(pte)   PGADDR(pte)
#define PGOFF(la)	    (((uintptr_t) (la)) & 0xFFF)

#define E_NO_MEM 1

#define HEAP_START 0x800000000UL
#define STACK_TOP  0x1000000000000UL

class MemorySystem
{
private:
    std::unordered_map<uintptr_t, pte_t> page_table;
    uintptr_t heap_pointer;

    std::vector<Cache*> cache;
    unsigned min_line_size;  // must be an power of 2, and >= 8
    Storage *inst_entry, *data_entry;
    Memory *memory;

    size_t total_memory_access_cycles;
    size_t memory_access_num;

    uintptr_t translate(reg_t ptr);

public:
    MemorySystem(const YAML::Node& cache_list, int memory_cycles);
    ~MemorySystem();
    void reset();
    pte_t page_alloc(uintptr_t va);
    void load_segment(FILE *file, const Elf64_Phdr& phdr);
    void write_str(uintptr_t va, const char *str);

    // return the number of cycles required
    int read_inst(reg_t ptr, inst_t& st);
    int read_data(reg_t ptr, reg_t& reg, int bytes);
    int write_data(reg_t ptr, reg_t reg, int bytes);
    uintptr_t sbrk(size_t size);

    void output_memory(uintptr_t va, char fm, char sz, size_t length);
    void print_info();

    void run_trace(const std::string& trace_file);
};

#endif
