#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>

typedef uint64_t reg_t;
typedef uint32_t inst_t;
typedef uint8_t reg_num_t;

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
template<class T>
inline T round_down(T a, uint64_t n)
{
    uint64_t _a = (uint64_t)a;
    return (T)(_a - _a % n);
}

// Round up to the nearest multiple of n
template<class T>
inline T round_up(T a, uint64_t n)
{
    return (T)round_down((uint64_t)a + n - 1, n);
}

template<class... Args>
static inline void throw_error(const char* __restrict format, Args... args)
{
    char msg[100];
    sprintf(msg, format, args...);
    throw std::runtime_error(msg);
}

#endif
