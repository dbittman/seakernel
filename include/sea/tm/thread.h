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

#define THREAD_KERNEL          0x1  /* a kernel thread */
#define THREAD_FORK            0x2  /* freshly made thread, hot from the oven! */
#define THREAD_TIMEOUT_EXPIRED 0x4  /* timeout expired while blocking */
#define THREAD_SIGNALED        0x8 /* scheduler has detected that a signal is meant for this
                                       task's userspace */
#define THREAD_SCHEDULE        0x10 /* we request a reschedule after this interrupt completes */

#define THREADSTATE_RUNNING 0
#define THREADSTATE_INTERRUPTIBLE 1
#define THREADSTATE_UNINTERRUPTIBLE 2
#define THREADSTATE_DEAD 3

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

const struct thread *arch_tm_get_current_thread(int) __attribute__((const));

int tm_thread_got_signal(struct thread *);
void tm_thread_enter_system(int sys);
void tm_thread_exit_system(void);
int sys_vfork(void);
void tm_thread_kill(struct thread *);
void tm_blocklist_wakeall(struct llist *blocklist);
void tm_thread_unblock(struct thread *t);
int tm_thread_block_timeout(struct llist *blocklist, time_t microseconds);
void tm_thread_set_state(struct thread *t, int state);
void tm_thread_add_to_blocklist(struct thread *t, struct llist *blocklist);
void tm_thread_remove_from_blocklist(struct thread *t);
int tm_thread_block(struct llist *blocklist, int state);
int tm_thread_reserve_usermode_stack(struct thread *thr);
void tm_thread_release_usermode_stack(struct thread *thr, int stack);
addr_t tm_thread_usermode_stack_end(int stack);
struct thread *tm_thread_get(pid_t tid);
int tm_thread_runnable(struct thread *thr);
void tm_thread_inc_reference(struct thread *thr);
void tm_thread_put(struct thread *thr);
void tm_thread_handle_signal(int signal);
void tm_signal_send_thread(struct thread *thr, int signal);
int sys_kill_thread(pid_t tid, int signal);
int tm_signal_will_be_fatal(struct thread *t, int sig);
void tm_thread_exit(int code);
void sys_exit(int);
pid_t tm_thread_next_tid(void);
struct thread *tm_thread_fork(int flags);
void tm_thread_add_to_process(struct thread *thr, struct process *proc);
void tm_thread_add_to_cpu(struct thread *thr, struct cpu *cpu);
int sys_clone(int flags);
void tm_schedule(void);
void tm_thread_user_mode_jump(void (*fn)(void));
void arch_tm_userspace_signal_initializer(registers_t *regs, struct sigaction *sa);
void arch_tm_userspace_signal_cleanup(registers_t *regs);
addr_t arch_tm_read_ip(void);
void arch_tm_jump_to_user_mode(addr_t jmp);

#define tm_thread_pause(th) tm_thread_set_state(th, THREADSTATE_INTERRUPTIBLE)
#define tm_thread_resume(th) tm_thread_set_state(th, THREADSTATE_RUNNING)

#endif

