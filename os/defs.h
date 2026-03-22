#ifndef DEFS_H
#define DEFS_H

#include "const.h"
#include "kalloc.h"
#include "log.h"
#include "printf.h"
#include "proc.h"
#include "riscv.h"
#include "sbi.h"
#include "string.h"
#include "types.h"
#include "vm.h"

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
uint64 sys_getpid(void);
uint64 sys_task_info(TaskInfo *info);
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd);
uint64 sys_munmap(uint64 start, uint64 len);
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
#endif // DEF_H
