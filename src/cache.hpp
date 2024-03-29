#ifndef CACHE_HPP
#define CACHE_HPP

#include <string>
#include <yaml-cpp/yaml.h>
#include "types.hpp"

class Storage
{
public:
    // return the number of cycles required
    virtual int read(uintptr_t ptr) = 0;
    virtual int write(uintptr_t ptr) = 0;
    virtual ~Storage() = default;
};

class Cache : public Storage
{
private:
    std::string name;
    int S, s, E, b;
    bool write_back;
    bool write_allocate;
    int hit_cycles;
    Storage *next;
    uint32_t time;
    uint64_t hit_num, miss_num;

    struct CacheLine
    {
        bool valid, dirty;
        uint32_t timestamp;
        uint64_t tag;
    } **cache_set;

    CacheLine* get_cache_line(uintptr_t ptr);

public:
    Cache(const YAML::Node& config);
    ~Cache();
    std::string get_name() const;
    unsigned get_line_size() const;
    void set_next(Storage *st);
    void invalidate();
    int read(uintptr_t ptr);
    int write(uintptr_t ptr);
    void print_info();
};

class Memory : public Storage
{
private:
    int cycles;

public:
    Memory(int cycles);
    int read(uintptr_t ptr);
    int write(uintptr_t ptr);
};

#endif
