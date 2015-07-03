#ifndef __SEA_TM_THREAD_H
#define __SEA_TM_THREAD_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/process.h>
#include <sea/tm/async_call.h>
#include <sea/tm/signal.h>
#include <sea/cpu/registers.h>
#include <sea/lib/hash.h>

#define KERN_STACK_SIZE 0x16000
#define THREAD_MAGIC 0xBABECAFE
#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

#define THREAD_RUNNING 0
#define THREAD_INTERRUPTIBLE 1
#define THREAD_UNINTERRUPTIBLE 2

#define tm_thread_raise_flag(t,f) or_atomic(&(t->flags), f)
#define tm_thread_lower_flag(t,f) and_atomic(&(t->flags), ~f)

struct thread {
	unsigned magic;
	pid_t tid;
	int cpuid;
	int state, flags;
	int system;
	int priority, timeslice;
	void *kernel_stack;
	unsigned long stack_pointer;

	sigset_t signal_mask;
	unsigned signal;
	registers_t *sysregs, *regs, regs_b;
	time_t stime, utime, t_cutime, t_cstime;

	struct llistnode blocknode, activenode, pnode;
	struct llist *blocklist;
	struct async_call block_timeout;
	struct process *process;
};

struct hash_table *thread_table;

#endif

