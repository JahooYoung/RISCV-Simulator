#include <new>
#include <cstring>
#include <cassert>
#include "memory_system.hpp"
using namespace std;

MemorySystem::~MemorySystem()
{
    for (const auto &pr: page_table)
        operator delete((void*)pr.second, align_val_t(PGSIZE));
}

pte_t MemorySystem::page_alloc(uintptr_t va)
{
    auto ptr = operator new(PGSIZE, align_val_t(PGSIZE));
    memset(ptr, 0, PGSIZE);
    page_table[PGADDR(va)] = (pte_t)ptr;
    return (pte_t)ptr;
}

void MemorySystem::load_segment(FILE *file, const Elf64_Phdr& phdr)
{
    uintptr_t end = phdr.p_vaddr + phdr.p_memsz;
    size_t sz = PGSIZE - phdr.p_vaddr % PGSIZE;
    fseek(file, phdr.p_offset, SEEK_SET);
    for (uintptr_t va = PGADDR(phdr.p_vaddr); va < end; va += PGSIZE) {
        pte_t pa = page_alloc(va);
        fread((void*)(pa + PGSIZE - sz), sz, 1, file);
    }
}

uintptr_t MemorySystem::translate(reg_t ptr)
{
    auto pte_p = page_table.find(PGADDR(ptr));
    if (pte_p == page_table.end())
        throw_error("invalid address: %lx", ptr);
    return PTE_ADDR(pte_p->second) | PGOFF(ptr);
}

int MemorySystem::read_inst(reg_t ptr, uint32_t& st)
{
    st = *(uint32_t*)translate(ptr);
    return 0;
}

int MemorySystem::read_data(reg_t ptr, reg_t& st)
{
    st = *(reg_t*)translate(ptr);
    return 0;
}

int MemorySystem::write_data(reg_t ptr, reg_t st, int bytes)
{
    auto pa = translate(ptr);
    switch (bytes) {
    case 1: *(uint8_t*)pa = (uint8_t)st; break;
    case 2: *(uint16_t*)pa = (uint16_t)st; break;
    case 4: *(uint32_t*)pa = (uint32_t)st; break;
    case 8: *(uint64_t*)pa = (uint64_t)st; break;
    }
    return 0;
}
