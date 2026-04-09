#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "proc.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

static uint64 time_in_ms()
{
	return get_cycle() / (CPU_FREQ / 1000);
}

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}
uint64 sys_getpid()
{
	return curr_proc()->pid;
}
__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}
// Updated for Project 2:
// Cannot directly write to user pointer anymore.
// Use copyout to safely copy time data to user memory.
////uses copy alt to usewith virtual memory 
uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	struct proc *p = curr_proc();
	TimeVal ktime;
	uint64 cycle;

	if (val == 0)
		return -1;

	cycle = get_cycle();
	ktime.sec = cycle / CPU_FREQ;
	ktime.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;

	if (copyout(p->pagetable, (uint64)val, (char *)&ktime, sizeof(TimeVal)) < 0)
		return -1;

	return 0;
}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
// Project 2:
// Returns information about the current task.
// Includes status, syscall counts, and running time.
// Uses copyout to safely send data to user space.
//uses copyout to safely send data to user space
uint64 sys_task_info(TaskInfo *info)
{
	struct proc *p = curr_proc();
	TaskInfo kinfo;

	if (info == 0)
		return -1;

	kinfo.status = p->task_status;
	for (int i = 0; i < MAX_SYSCALL_NUM; i++) {
		kinfo.syscall_times[i] = p->syscall_times[i];
	}
	kinfo.time = (int)(time_in_ms() - p->start_time);

	if (copyout(p->pagetable, (uint64)info, (char *)&kinfo, sizeof(TaskInfo)) < 0)
		return -1;

	return 0;
} 
// Project 2:
// mmap system call
// Allocates memory pages and maps them into the process address space.
// Converts user permissions into page table flags (R/W/X).
// Uses kalloc to allocate physical memory and mappages to map it.
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
	struct proc *p = curr_proc();
	uint64 size, va;
	int perm = PTE_U;

	if (len == 0)
		return 0;
	if (start % PGSIZE != 0)
		return -1;

	// valid permission bits: R=1, W=2, X=4
	if (port < 1 || port > 7)
		return -1;

	if (port & 0x1)
		perm |= PTE_R;
	if (port & 0x2)
		perm |= PTE_W;
	if (port & 0x4)
		perm |= PTE_X;

	size = PGROUNDUP(len);
//allocate physical page and map t virtual memory 
	for (va = start; va < start + size; va += PGSIZE) {
		void *pa = kalloc();
		if (pa == 0)
			return -1;

		memset(pa, 0, PGSIZE);

		if (mappages(p->pagetable, va, PGSIZE, (uint64)pa, perm) < 0) {
			kfree(pa);
			return -1;
		}
	}

	// keep max_page updated for later freeing
	if ((start + size) / PAGE_SIZE > p->max_page)
		p->max_page = (start + size) / PAGE_SIZE;

	return 0;
}
// Project 2:
// munmap system call
// Unmaps previously allocated memory pages from the process.
// Frees physical memory and removes mappings.
uint64 sys_munmap(uint64 start, uint64 len)
{
	struct proc *p = curr_proc();
	uint64 size, va;

	if (len == 0)
		return 0;
	if (start % PGSIZE != 0)
		return -1;

	size = PGROUNDUP(len);

	for (va = start; va < start + size; va += PGSIZE) {
		pte_t *pte = walk(p->pagetable, va, 0);
		if (pte == 0 || (*pte & PTE_V) == 0)
			return -1;
	}

	uvmunmap(p->pagetable, start, size / PGSIZE, 1);
	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	if (id >= 0 && id < MAX_SYSCALL_NUM) {
		curr_proc()->syscall_times[id]++;
	}	
		   /*
	* LAB1: you may need to update syscall counter for task info here
	*/
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}

	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
