#include "cache.hpp"
using namespace std;

static inline int log2(int x)
{
    int ret = 0;
    for (; x > 1; x >>= 1)
        ret++;
    return ret;
}

Cache::Cache(const YAML::Node& config)
{
    int size, line_size;
    try {
        name = config["name"].as<string>();
        size = config["size"].as<int>() * 1024;  // KB => Bytes
        E = config["associativity"].as<int>();
        line_size = config["cache_line_bytes"].as<int>(64);
        write_back = config["write_back"].as<bool>(true);
        write_allocate = config["write_allocate"].as<bool>(true);
        hit_cycles = config["hit_cycles"].as<int>();
    } catch (const YAML::BadConversion&) {
        printf("cache config error\n");
        exit(EXIT_FAILURE);
    }
    S = size / line_size / E;
    s = log2(S);
    b = log2(line_size);

    cache_set = new CacheLine*[S];
    for (int i = 0; i < S; i++)
        cache_set[i] = new CacheLine[E];
}

Cache::~Cache()
{
    for (int i = 0; i < S; i++)
        delete[] cache_set[i];
    delete[] cache_set;
}

string Cache::get_name() const
{
    return name;
}

unsigned Cache::get_line_size() const
{
    return 1U << b;
}

void Cache::set_next(Storage *st)
{
    next = st;
}

void Cache::invalidate()
{
    hit_num = miss_num = 0;
    for (int i = 0; i < S; i++)
        for (int j = 0; j < E; j++)
            cache_set[i][j].valid = false;
}

Cache::CacheLine* Cache::get_cache_line(uintptr_t ptr)
{
    auto set = cache_set[(ptr >> b) & (S - 1)];
    uint64_t tag = ptr >> (b + s);
    CacheLine *evict = nullptr;
    for (int i = 0; i < E; i++)
        if (set[i].valid && set[i].tag == tag) {
            return set + i;
        } else {
            if (!evict || !set[i].valid ||
                time - set[i].timestamp > time - evict->timestamp)
                evict = set + i;
        }
    return evict;
}

int Cache::read(uintptr_t ptr)
{
    time++;
    uint64_t tag = ptr >> (b + s);
    auto line = get_cache_line(ptr);
    if (line->valid && line->tag == tag) {
        // read hit
        hit_num++;
        line->timestamp = time;
        return hit_cycles;
    }
    miss_num++;
    int cycles = hit_cycles;
    if (write_back && line->valid && line->dirty)
        cycles += next->write((line->tag << (b + s)) | (ptr & ((S - 1) << b)));
    cycles += next->read(ptr);
    line->valid = true;
    line->dirty = false;
    line->timestamp = time;
    line->tag = tag;
    return cycles;
}

int Cache::write(uintptr_t ptr)
{
    time++;
    uint64_t tag = ptr >> (b + s);
    auto line = get_cache_line(ptr);
    if (line->valid && line->tag == tag) {
        // write hit
        hit_num++;
        line->dirty = true;
        line->timestamp = time;
        int cycles = hit_cycles;
        if (!write_back)
            cycles += next->write(ptr);
        return cycles;
    }
    miss_num++;
    if (!write_allocate)
        return next->write(ptr);
    int cycles = hit_cycles;
    if (write_back && line->valid && line->dirty)
        cycles += next->write((line->tag << (b + s)) | (ptr & ((S - 1) << b)));
    cycles += next->read(ptr);
    line->valid = true;
    line->dirty = true;
    line->timestamp = time;
    line->tag = tag;
    return cycles;
}

void Cache::print_info()
{
    printf("%20s: hit=%-10lu miss=%-10lu miss_rate=%.3f%%\n", name.c_str(),
        hit_num, miss_num, (double)miss_num / (hit_num + miss_num) * 100);
}


Memory::Memory(int cycles)
    : cycles(cycles)
{}

int Memory::read(uintptr_t ptr)
{
    return cycles;
}

int Memory::write(uintptr_t ptr)
{
    return cycles;
}
