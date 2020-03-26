#include <stdint.h>
#include <syscall.h>
#include <tinylib.h>

static inline int64_t
syscall(int num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	int64_t ret;

	// Generic system call: pass system call number in a7,
	// up to five parameters in a1, a2, a3, a4, a5.
	// Interrupt kernel with `ecall`.
	//
	// The "volatile" tells the assembler not to optimize
	// this instruction away just because we don't use the
	// return value.
	//
	// The last clause tells the assembler that this can
	// potentially change arbitrary memory locations.

	asm volatile(
        "mv a1, %1\n"
        "mv a2, %2\n"
        "mv a3, %3\n"
        "mv a4, %4\n"
        "mv a5, %5\n"
        "li a7, %6\n"
        "ecall\n"
        "mv %1, a0"
        :   "=r" (ret)
        :   "r" (a1),
            "r" (a2),
            "r" (a3),
            "r" (a4),
            "r" (a5),
            "i" (num)
        :   "memory"
    );

	return ret;
}

void sys_cputchar(int ch)
{
	syscall(SYS_cputchar, (uint64_t)ch, 0, 0, 0, 0);
}

void *sys_sbrk(size_t size)
{
    return (void*)syscall(SYS_sbrk, (uint64_t)size, 0, 0, 0, 0);
}

int sys_readint(void)
{
    return (int)syscall(SYS_readint, 0, 0, 0, 0, 0);
}

long sys_time(void)
{
    return (long)syscall(SYS_time, 0, 0, 0, 0, 0);
}
