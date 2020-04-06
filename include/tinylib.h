#ifndef SIM_INCLUDE_TINYLIB_H
#define SIM_INCLUDE_TINYLIB_H

#include <tinyarg.h>

typedef unsigned long size_t;

// lib/printfmt.c
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);

// lib/printf.c
#define printf(...) tprintf(__VA_ARGS__)
int	tprintf(const char *fmt, ...);
int	vcprintf(const char *fmt, va_list);

// lib/malloc.c
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);

// lib/syscall.c
void sys_cputchar(int ch);
void *sys_sbrk(size_t size);

#define readint() sys_readint()
int sys_readint(void);

#define time() sys_time()
long sys_time(void);

// lib/util.c
void srand(unsigned int seed);
int rand(void);

#define isdigit(ch) tisdigit(ch)
int tisdigit(char ch);
int atoi(const char *str);

#define assert(expr) _assert(#expr, expr)
void _assert(char const* expr, int value);

#endif /* !SIM_INCLUDE_TINYLIB_H */
