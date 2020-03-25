#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>

typedef uint64_t reg_t;
typedef uint32_t inst_t;

template<class... Args>
static inline void throw_error(const char* __restrict format, Args... args)
{
    char msg[100];
    sprintf(msg, format, args...);
    throw std::runtime_error(msg);
}

#endif
