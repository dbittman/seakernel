#ifndef __SEA_TM_THREAD_H
#define __SEA_TM_THREAD_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/process.h>
#include <sea/tm/async_call.h>
#include <sea/tm/signal.h>
#include <sea/cpu/registers.h>

struct thread {
	unsigned magic;
	pid_t tid;
	int cpuid;
	int state, flags;
	int system;
	int priority, timeslice;
	void *kernel_stack;

	sigset_t signal_mask;
	unsigned signal;
	registers_t *sysregs, *regs, regs_b;
	time_t stime, utime, t_cutime, t_cstime;

	struct llistnode blocknode, activenode, pnode;
	struct llist *blocklist;
	struct async_call block_timeout;
	struct process *process;
};

#endif

