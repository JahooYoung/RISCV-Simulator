#include <new>
#include <cstring>
#include <cassert>
#include <map>
#include "memory_system.hpp"
using nlohmann::json;
using namespace std;

MemorySystem::MemorySystem(const json& cache_list, int memory_cycles)
    : heap_pointer(HEAP_START)
{
    map<string, Storage*> storage_map;
    inst_entry = data_entry = memory = new Memory(memory_cycles);
    storage_map["memory"] = memory;
    min_line_size = PGSIZE;
    for (auto &conf: cache_list) {
        auto st = new Cache(conf);
        cache.push_back(st);
        storage_map[st->get_name()] = st;
        min_line_size = min(min_line_size, st->get_line_size());
        if (conf.value("instruction_entry", false))
            inst_entry = st;
        if (conf.value("data_entry", false))
            data_entry = st;
    }

    for (auto &conf: cache_list) {
        auto st = dynamic_cast<Cache*>(storage_map[conf["name"].get<string>()]);
        st->set_next(storage_map[conf["cache_for"].get<string>()]);
    }
}

MemorySystem::~MemorySystem()
{
    for (auto c: cache)
        delete c;
    for (const auto &pr: page_table)
        operator delete((void*)pr.second, align_val_t(PGSIZE));
}

void MemorySystem::reset()
{
    heap_pointer = HEAP_START;
    for (const auto &pr: page_table)
        operator delete((void*)pr.second, align_val_t(PGSIZE));
    page_table.clear();

    for (auto c: cache)
        c->invalidate();
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
    size_t filesz = phdr.p_filesz;
    fseek(file, phdr.p_offset, SEEK_SET);
    for (uintptr_t va = phdr.p_vaddr; va < end; va = PGADDR(va) + PGSIZE) {
        page_alloc(va);
        size_t sz = min(PGSIZE - va % PGSIZE, filesz);
        fread((void*)translate(va), sz, 1, file);
        filesz -= sz;
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
    if ((ptr & (PGSIZE - 1)) == 0xFFE) {
        st = *(uint16_t*)translate(ptr);
        st |= (uint32_t)*(uint16_t*)translate(ptr + 2) << 16;
    } else {
        st = *(uint32_t*)translate(ptr);
    }

    // get cycles num
    int cycles = inst_entry->read(translate(ptr));
    if ((ptr & (min_line_size - 1)) > min_line_size - 4)
        cycles += inst_entry->read(translate(ptr + 2));
    return cycles;
}

int MemorySystem::read_data(reg_t ptr, reg_t& reg, int bytes)
{
    if ((ptr & (PGSIZE - 1)) > PGSIZE - bytes) {
        reg = 0;
        for (int i = 0; i < bytes; i++)
            reg |= (reg_t)*(uint8_t*)translate(ptr + i) << (i * 8);
    } else {
        auto pa = translate(ptr);
        switch (bytes) {
        case 1: reg = *(uint8_t*)pa; break;
        case 2: reg = *(uint16_t*)pa; break;
        case 4: reg = *(uint32_t*)pa; break;
        case 8: reg = *(uint64_t*)pa; break;
        }
    }

    // get cycles num
    int cycles = data_entry->read(translate(ptr));
    if ((ptr & (min_line_size - 1)) > min_line_size - bytes)
        cycles += data_entry->read(translate(ptr + bytes - 1));
    return cycles;
}

int MemorySystem::write_data(reg_t ptr, reg_t reg, int bytes)
{
    if ((ptr & (PGSIZE - 1)) > PGSIZE - bytes) {
        for (int i = 0; i < bytes; i++)
            *(uint8_t*)translate(ptr + i) = (reg >> (i * 8)) & 0xFF;
    } else {
        auto pa = translate(ptr);
        switch (bytes) {
        case 1: *(uint8_t*)pa = (uint8_t)reg; break;
        case 2: *(uint16_t*)pa = (uint16_t)reg; break;
        case 4: *(uint32_t*)pa = (uint32_t)reg; break;
        case 8: *(uint64_t*)pa = (uint64_t)reg; break;
        }
    }

    // get cycles num
    int cycles = data_entry->write(translate(ptr));
    if ((ptr & (min_line_size - 1)) > min_line_size - bytes)
        cycles += data_entry->write(translate(ptr + bytes - 1));
    return cycles;
}

uintptr_t MemorySystem::sbrk(size_t size)
{
    uintptr_t old_heap_pointer = heap_pointer;
    uintptr_t end = heap_pointer + size;
    while (heap_pointer < end) {
        if (page_table.find(PGADDR(heap_pointer)) == page_table.end())
            page_alloc(heap_pointer);
        heap_pointer = PGADDR(heap_pointer) + PGSIZE;
    }
    heap_pointer = end;
    return old_heap_pointer;
}

#define print_mem(type) {                            \
    for (size_t i = 0; i < length; i++) {            \
        if (i % 8 == 0)                              \
            printf("\n%p:", (type*)va + i);          \
        printf(format, *(type*)translate(va + i * sizeof(type)));   \
    }                                                \
    printf("\n"); }

void MemorySystem::output_memory(uintptr_t va, char fm, char sz, size_t length)
{
    try {
        if (fm == 's') {
            printf("%p: %s\n", (char*)va, (char*)translate(va));
            return;
        }
        char format[10] = " %d";
        format[2] = fm;
        switch (sz) {
        case 'b': print_mem(uint8_t) break;
        case 'h': print_mem(uint16_t) break;
        case 'w':
            if (fm == 'f')
                print_mem(float)
            else
                print_mem(uint32_t)
            break;
        case 'g':
            if (fm == 'f')
                print_mem(double)
            else {
                strcpy(format, " %ld");
                format[3] = fm;
                print_mem(uint64_t)
            }
            break;
        }
    } catch (runtime_error err) {
        printf("\nerror: %s\n", err.what());
    }
}

void MemorySystem::print_info()
{
    printf("    heap_pointer: %lx\n", heap_pointer);
}
