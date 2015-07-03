#ifndef _SEA_TM_PROCESS_H
#define _SEA_TM_PROCESS_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/cpu/registers.h>
#include <sea/tm/signal.h>
#include <sea/mm/context.h>
#include <sea/sys/stat.h>
#include <sea/mm/valloc.h>
#include <sea/tm/async_call.h>
#include <sea/lib/hash.h>

#define current_process current_thread->process

#define FILP_HASH_LEN 512

/* exit reasons */
#define __EXIT     0
#define __COREDUMP 1
#define __EXITSIG  2
#define __STOPSIG  4
#define __STOPPED  8

#define TASK_MAGIC 0xCAFEBABE

/* flags for different task states */
#define TF_INSIG        0x800 /* inside a signal */
#define TF_SCHED       0x1000 /* we request a reschedule after this syscall completes */
#define TF_JUMPIN      0x2000 /* going to jump to a signal handler on the next IRET
* used by the syscall handler to determine if it needs to back up
* it's return value */
#define TF_IN_INT     0x40000 /* inside an interrupt handler */
#define TF_BGROUND    0x80000 /* is waiting for things in the kernel (NOT blocking for a resource) */

typedef struct exit_status {
	unsigned pid;
	int ret;
	unsigned sig;
	char coredump;
	int cause;
	struct exit_status *next, *prev;
} ex_stat;

struct process {
	unsigned magic;
	vmm_context_t *mm_context;
	pid_t pid;
	int flags;

	struct llistnode listnode;

	addr_t heap_start, heap_end;
	char command[128];
	char **argv, **env;
	int cmask;
	int tty;

	sigset_t signal_mask;
	unsigned signal;
	struct process *parent;
	struct llist threadlist;
};

#define WNOHANG 1

#define FORK_SHAREDIR 0x1
#define FORK_SHAREDAT 0x2
#define tm_fork() tm_do_fork(0)

extern unsigned running_processes;

int tm_set_gid(int n);
int tm_set_uid(int n);
int tm_set_euid(int n);
int tm_set_egid(int n);
int tm_get_gid();
int tm_get_uid();
int tm_get_egid();
int tm_get_euid();

void arch_tm_set_kernel_stack(addr_t, addr_t);
void tm_set_kernel_stack(addr_t, addr_t);

int sys_times(struct tms *buf);
int sys_waitpid(int pid, int *st, int opt);
int sys_wait3(int *, int, int *);

int sys_getppid();
int sys_alarm(int a);
int sys_gsetpriority(int set, int which, int id, int val);
int sys_waitagain();
int sys_nice(int which, int who, int val, int flags);
int sys_setsid();
int sys_setpgid(int a, int b);
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
	struct timeval *timeout);
int sys_sbrk(long inc);
int sys_isstate(int pid, int state);

/* provided by arch-dep code */
extern void arch_do_switch_to_user_mode();
void arch_tm_set_current_thread_marker(addr_t *space, addr_t task);

struct hash_table *process_table;

#include <sea/mm/vmm.h>

#define MMF_VMEM_NUM_INDEX_PAGES 12

#endif
