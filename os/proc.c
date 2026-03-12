#include "proc.h"
#include "defs.h"
#include "loader.h"
#include "timer.h"
#include "trap.h"

struct proc pool[NPROC];
char kstack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char ustack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char trapframe[NPROC][PAGE_SIZE];

extern char boot_stack_top[];
struct proc *current_proc;
struct proc idle;

int threadid()
{
	return curr_proc()->pid;
}

struct proc *curr_proc()
{
	return current_proc;
}

// initialize the proc table at boot time.
void proc_init(void)
{
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		p->state = UNUSED;
		p->kstack = (uint64)kstack[p - pool];
		p->ustack = (uint64)ustack[p - pool];
		p->trapframe = (struct trapframe *)trapframe[p - pool];

		/*
		 * LAB1: initialize new fields
		 */
		p->task_status = UnInit;
		memset(p->syscall_times, 0, sizeof(p->syscall_times));
		p->start_time = 0;
	}
	idle.kstack = (uint64)boot_stack_top;
	idle.pid = 0;
	current_proc = &idle;
}

int allocpid()
{
	static int PID = 1;
	return PID++;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *allocproc(void)
{
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		if (p->state == UNUSED) {
			goto found;
		}
	}
	return 0;

found:
	p->pid = allocpid();
	p->state = USED;
	memset(&p->context, 0, sizeof(p->context));
	memset(p->trapframe, 0, PAGE_SIZE);
	memset((void *)p->kstack, 0, PAGE_SIZE);
	p->context.ra = (uint64)usertrapret;
	p->context.sp = p->kstack + PAGE_SIZE;

	p->task_status = Ready;
	memset(p->syscall_times, 0, sizeof(p->syscall_times));
	p->start_time = 0;

	return p;
}

// Scheduler never returns.
void scheduler(void)
{
	struct proc *p;
	for (;;) {
		for (p = pool; p < &pool[NPROC]; p++) {
			if (p->state == RUNNABLE) {
				p->task_status = Running;
				p->state = RUNNING;
				current_proc = p;
				swtch(&idle.context, &p->context);
			}
		}
	}
}

// Switch to scheduler.
void sched(void)
{
	struct proc *p = curr_proc();
	if (p->state == RUNNING)
		panic("sched running");
	swtch(&p->context, &idle.context);
}

// Give up the CPU for one scheduling round.
void yield(void)
{
	struct proc *p = curr_proc();
	p->task_status = Ready;
	p->state = RUNNABLE;
	sched();
}

// Exit the current process.
void exit(int code)
{
	struct proc *p = curr_proc();
	p->task_status = Exited;

	infof("proc %d exit with %d", p->pid, code);
	p->state = UNUSED;
	finished();
	sched();
}