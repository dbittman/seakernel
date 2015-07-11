#ifndef __SEA_TM_THREAD_H
#define __SEA_TM_THREAD_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/async_call.h>
#include <sea/tm/signal.h>
#include <sea/cpu/registers.h>
#include <sea/lib/hash.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>

#define KERN_STACK_SIZE 0x1000
#define THREAD_MAGIC 0xBABECAFE
#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

/* TODO: rename these to THREAD_ */
#define TF_SHUTDOWN        0x1  /* TODO: do we need all these? */
#define TF_KTASK           0x2  /* a kernel thread */
#define TF_FORK            0x4  /* freshly made thread, hot from the oven! */
#define TF_TIMEOUT_EXPIRED 0x8  /* timeout expired while blocking */
#define TF_SIGNALED        0x10 /* scheduler has detected that a signal is meant for this
								   task's userspace */
#define TF_SCHED           0x20 /* we request a reschedule after this interrupt completes */

#define THREAD_RUNNING 0
#define THREAD_INTERRUPTIBLE 1
#define THREAD_UNINTERRUPTIBLE 2
#define THREAD_DEAD 3

#define tm_thread_raise_flag(t,f) or_atomic(&(t->flags), f)
#define tm_thread_lower_flag(t,f) and_atomic(&(t->flags), ~f)

#define current_thread ((struct thread *)arch_tm_get_current_thread(kernel_state_flags))

struct process;
struct thread {
	unsigned magic;
	pid_t tid;
	int refs;
	int cpuid;
	int state, flags;
	int system;
	int priority, timeslice;
	void *kernel_stack;
	unsigned long stack_pointer, jump_point;
	addr_t usermode_stack_start;
	addr_t usermode_stack_end;
	int usermode_stack_num;
	struct cpu *cpu;

	sigset_t sig_mask, old_mask;
	unsigned signal;
	registers_t *regs;
	time_t stime, utime, t_cutime, t_cstime;

	struct llistnode blocknode, activenode, pnode;
	struct llist *blocklist;
	struct async_call block_timeout;
	struct process *process;
};

extern size_t running_threads;
extern struct hash_table *thread_table;

/* TODO: const */
const struct thread *arch_tm_get_current_thread(int) __attribute__((const));

int tm_thread_got_signal(struct thread *);
void tm_thread_enter_system(int sys);
void tm_thread_exit_system();
int tm_do_fork(int);
int sys_vfork();
void tm_set_signal(int sig, addr_t hand);
void tm_exit(int);
void tm_thread_kill(struct thread *);
void tm_blocklist_wakeall(struct llist *blocklist);
void tm_thread_unblock(struct thread *t);
int tm_thread_block_timeout(struct llist *blocklist, time_t microseconds);
void tm_thread_set_state(struct thread *t, int state);
void tm_thread_add_to_blocklist(struct thread *t, struct llist *blocklist);
void tm_thread_remove_from_blocklist(struct thread *t);
int tm_thread_block(struct llist *blocklist, int state);
void tm_switch_to_user_mode();
void tm_thread_delay_sleep();
int tm_thread_reserve_usermode_stack(struct thread *thr);
void tm_thread_release_usermode_stack(struct thread *thr, int stack);
addr_t tm_thread_usermode_stack_end(int stack);

#define tm_thread_pause(th) tm_thread_set_state(th, THREAD_INTERRUPTIBLE)
#define tm_thread_resume(th) tm_thread_set_state(th, THREAD_RUNNING)

#endif

