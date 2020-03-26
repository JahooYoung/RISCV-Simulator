#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#include <stddef.h>

/* system call numbers */
enum {
	SYS_cputchar = 0,
    SYS_sbrk,
    SYS_readint,
    SYS_time,
    SYS_exit = 93,
	NSYSCALLS
};

#endif /* !JOS_INC_SYSCALL_H */
