#ifndef __SEA_TM_THREAD_H
#define __SEA_TM_THREAD_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/process.h>
#include <sea/tm/async_call.h>
#include <sea/tm/signal.h>
#include <sea/cpu/registers.h>
#include <sea/lib/hash.h>
#include <sea/cpu/processor.h>

#define KERN_STACK_SIZE 0x16000
#define THREAD_MAGIC 0xBABECAFE
#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

/* TODO: rename these to THREAD_ */
#define TF_SHUTDOWN 1 /* TODO: do we need all these? */
#define TF_KTASK 2
#define TF_FORK  4
#define TF_TIMEOUT_EXPIRED 4
/* flags for different task states */
#define TF_INSIG        0x800 /* inside a signal */
#define TF_SCHED       0x1000 /* we request a reschedule after this syscall completes */
#define TF_JUMPIN      0x2000 /* going to jump to a signal handler on the next IRET
* used by the syscall handler to determine if it needs to back up
* it's return value */
#define TF_IN_INT     0x40000 /* inside an interrupt handler */
#define TF_BGROUND    0x80000 /* is waiting for things in the kernel (NOT blocking for a resource) */


#define THREAD_RUNNING 0
#define THREAD_INTERRUPTIBLE 1
#define THREAD_UNINTERRUPTIBLE 2

#define tm_thread_raise_flag(t,f) or_atomic(&(t->flags), f)
#define tm_thread_lower_flag(t,f) and_atomic(&(t->flags), ~f)

#define current_thread __current_cpu->cur
struct thread {
	unsigned magic;
	pid_t tid;
	int cpuid;
	int state, flags;
	int system;
	int priority, timeslice;
	void *kernel_stack;
	unsigned long stack_pointer;
	struct cpu *cpu;

	sigset_t sig_mask, old_mask;
	unsigned signal;
	struct sigaction signal_act[128]; /* TODO: per thread? */
	registers_t *sysregs, *regs, regs_b;
	time_t stime, utime, t_cutime, t_cstime;

	struct llistnode blocknode, activenode, pnode;
	struct llist *blocklist;
	struct async_call block_timeout;
	struct process *process;
};

extern size_t running_threads;
extern struct hash_table *thread_table;

int tm_thread_got_signal(struct thread *);
void tm_thread_enter_system(int sys);
void tm_thread_exit_system();
void tm_thread_delay_sleep(time_t nanoseconds);
void tm_thread_delay(time_t nanoseconds);
int tm_do_fork(int);
int sys_vfork();
void tm_set_signal(int sig, addr_t hand);
void tm_exit(int);
void tm_kill_thread(struct thread *);
void tm_blocklist_wakeall(struct llist *blocklist);
void tm_thread_unblock(struct thread *t);
int tm_thread_block_timeout(struct llist *blocklist, time_t nanoseconds);
void tm_thread_set_state(struct thread *t, int state);
void tm_thread_add_to_blocklist(struct thread *t, struct llist *blocklist);
void tm_thread_remove_from_blocklist(struct thread *t);
int tm_thread_block(struct llist *blocklist, int state);
void tm_switch_to_user_mode();
#define ONE_SECOND 1000 /* TODO: set this to the correct value */

#define tm_thread_pause(th) tm_thread_set_state(th, THREAD_INTERRUPTIBLE)
#define tm_thread_resume(th) tm_thread_set_state(th, THREAD_RUNNING)

#endif

