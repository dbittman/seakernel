#ifndef __SEA_TM_THREAD_H
#define __SEA_TM_THREAD_H

#include <sea/types.h>
#include <sea/lib/linkedlist.h>
#include <sea/kernel.h>
#include <sea/tm/async_call.h>
#include <sea/tm/signal.h>
#include <sea/cpu/registers.h>
#include <sea/lib/hash.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sea/arch-include/tm-thread.h>
#include <sea/mm/valloc.h>
#include <sea/tm/workqueue.h>
#include <sea/spinlock.h>
#include <sea/lib/linkedlist.h>
#include <sea/cpu/processor.h>
#define KERN_STACK_SIZE 0x20000
#define THREAD_MAGIC 0xBABECAFE
#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

#define THREAD_KERNEL          0x1  /* a kernel thread */
#define THREAD_TIMEOUT_EXPIRED 0x2  /* timeout expired while blocking */
#define THREAD_SIGNALED        0x4  /* scheduler has detected that a signal is meant for this
                                       task's userspace */
#define THREAD_SCHEDULE        0x8  /* we request a reschedule after this interrupt completes */
#define THREAD_EXIT            0x10 /* we plan to exit when returning from this interrupt */
#define THREAD_WAKEUP          0x20 /* the scheduler won't let the state -> THREADSTATE_INTERRUPTIBLE
									   if this flag is set, but it will then reset the flag */
#define THREAD_DEAD            0x40 /* thread is dead, AND has scheduled away */
#define THREAD_PTRACED         0x80 /* this thread is being traced */
#define THREAD_TICKER_DOWORK   0x100 /* when returning from this interrupt, do work in ticker */

#define THREADSTATE_RUNNING 0
#define THREADSTATE_INTERRUPTIBLE 1
#define THREADSTATE_UNINTERRUPTIBLE 2
#define THREADSTATE_DEAD 3
#define THREADSTATE_STOPPED 4

#define tm_thread_raise_flag(t,f) atomic_fetch_or(&(t->flags), f)
#define tm_thread_lower_flag(t,f) atomic_fetch_and(&(t->flags), ~f)

#define current_thread ((struct thread *)*arch_tm_get_current_thread())
const struct thread **arch_tm_get_current_thread(void) __attribute__((const));
/* warning: this is not "safe" to use! The cpu could change on
 * any schedule, which may happen while you're using this. Use
 * cpu_get_current to make sure that doesn't happen. */
#define __current_cpu ((struct cpu *)current_thread->cpu)

struct process;
struct cpu;
struct thread {
	unsigned magic;
	pid_t tid;
	_Atomic int refs;
	int cpuid;
	int state;
	_Atomic int flags;
	uint64_t system;
	int interrupt_level;
	int priority, timeslice;
	int exit_code;
	addr_t kernel_stack;
	addr_t stack_pointer, jump_point;
	addr_t usermode_stack_end, usermode_stack_start;
	int stack_num;
	struct cpu *cpu;
	int held_locks;

	sigset_t sig_mask;
	unsigned signal, signals_pending;
	struct registers *regs;

	struct arch_thread_data arch_thread;

	struct linkedentry activenode;
	struct linkedentry pnode;
	struct linkedentry blocknode;
	_Atomic struct blocklist *blocklist;
	struct spinlock status_lock;
	struct async_call block_timeout;
	struct async_call alarm_timeout;
	struct async_call cleanup_call;
	struct async_call waitcheck_call;
	struct async_call blockreq_call;
	_Atomic struct ticker *alarm_ticker;
	struct process *process;
	struct workqueue resume_work;
	struct kthread *kernel_thread;
	struct hashelem hash_elem;
	/* ptrace */
	struct thread *tracer;
	int tracee_flags;
	unsigned long orig_syscall;
	long syscall_return;
};

extern size_t running_threads;
extern struct hash *thread_table;

int tm_thread_got_signal(struct thread *);
int tm_clone(int, void *entry, struct kthread *);
void tm_thread_enter_system(int sys);
void tm_thread_exit_system(long, long);
int sys_vfork(void);
void tm_thread_kill(struct thread *);
void tm_thread_unblock(struct thread *t);
void tm_thread_set_state(struct thread *t, int state);
void tm_thread_poke(struct thread *t);
int sys_thread_setpriority(pid_t tid, int val, int flags);
struct thread *tm_thread_get(pid_t tid);
int tm_thread_runnable(struct thread *thr);
void tm_thread_inc_reference(struct thread *thr);
void tm_thread_put(struct thread *thr);
void tm_thread_handle_signal(int signal);
int sys_delay(long);
void tm_signal_send_thread(struct thread *thr, int signal);
int sys_kill_thread(pid_t tid, int signal);
int tm_signal_will_be_fatal(struct thread *t, int sig);
void tm_thread_exit(int code);
void tm_thread_do_exit(void);
void sys_exit(int);
pid_t tm_thread_next_tid(void);
struct thread *tm_thread_fork(int flags);
void tm_thread_add_to_process(struct thread *thr, struct process *proc);
void tm_thread_add_to_cpu(struct thread *thr, struct cpu *cpu);
int sys_clone(int flags);
void tm_schedule(void);
void tm_thread_user_mode_jump(void (*fn)(void));
void arch_tm_userspace_signal_initializer(struct registers *regs, struct sigaction *sa);
void arch_tm_userspace_signal_cleanup(struct registers *regs);
addr_t arch_tm_read_ip(void);
void arch_tm_jump_to_user_mode(addr_t jmp);
__attribute__((noinline)) void arch_tm_thread_switch(struct thread *old, struct thread *, addr_t);
__attribute__((noinline)) void arch_tm_fork_setup_stack(struct thread *thr);
int tm_thread_delay(time_t microseconds);
void tm_thread_create_kerfs_entries(struct thread *thr);
bool tm_thread_reserve_stacks(struct thread *thr);
void tm_thread_release_stacks(struct thread *thr);

#define tm_thread_pause(th) tm_thread_set_state(th, THREADSTATE_INTERRUPTIBLE)
#define tm_thread_resume(th) tm_thread_set_state(th, THREADSTATE_RUNNING)

extern struct valloc km_stacks;
#endif

