#include <new>
#include <cstring>
#include <cassert>
#include "memory_system.hpp"
using namespace std;

inline void* page_alloc()
{
    auto ptr = operator new(PGSIZE, align_val_t(PGSIZE));
    memset(ptr, 0, PGSIZE);
    return ptr;
}

MemorySystem::~MemorySystem()
{
    for (const auto &pr: page_table)
        operator delete((void*)pr.second, align_val_t(PGSIZE));
}

void MemorySystem::load_segment(FILE *file, const Elf64_Phdr& phdr)
{
    uint32_t end = phdr.p_vaddr + phdr.p_memsz;
    uint32_t sz = PGSIZE - phdr.p_vaddr % PGSIZE;
    fseek(file, phdr.p_offset, SEEK_SET);
    for (uintptr_t va = phdr.p_vaddr & ~PGSIZE; va < end; va += PGSIZE) {
        pte_t pa = (pte_t)page_alloc();
        page_table[va] = pa;
        fread((void*)(pa + PGSIZE - sz), sz, 1, file);
    }
}

int MemorySystem::read_inst(reg_t ptr, uint32_t& st)
{
    auto pte_p = page_table.find(PGADDR(ptr));
    if (pte_p == page_table.end())
        return -1;
    st = *(uint32_t*)(PTE_ADDR(pte_p->second) | PGOFF(ptr));
    return 0;
}

int MemorySystem::read_data(reg_t ptr, reg_t& st)
{
    auto pte_p = page_table.find(PGADDR(ptr));
    if (pte_p == page_table.end())
        return -1;
    st = *(reg_t*)(PTE_ADDR(pte_p->second) | PGOFF(ptr));
    return 0;
}
