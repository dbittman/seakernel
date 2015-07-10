#ifndef _SEA_TM_PROCESS_H
#define _SEA_TM_PROCESS_H

#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/ll.h>
#include <sea/cpu/registers.h>
#include <sea/tm/signal.h>
#include <sea/sys/stat.h>
#include <sea/mm/valloc.h>
#include <sea/mm/context.h>
#include <sea/tm/async_call.h>
#include <sea/lib/hash.h>
#include <sea/tm/thread.h>

#define current_process (__tm_get_current_process())

/* TODO: const? */
static inline struct process *__tm_get_current_process(void)
{
	struct thread *thr = current_thread;
	if(thr == 0)
		return 0;
	return thr->process;
}

#define FILP_HASH_LEN 512

/* exit reasons */
#define __EXIT     0
#define __COREDUMP 1
#define __EXITSIG  2
#define __STOPSIG  4
#define __STOPPED  8

#define PROCESS_MAGIC 0xCAFEBABE

#define PROCESS_FORK 1
#define PROCESS_CLEANED 2

struct exit_status {
	unsigned pid;
	int ret;
	unsigned sig;
	char coredump;
	int cause;
	struct exit_status *next, *prev;
};

#define NUM_USERMODE_STACKS ((USERMODE_STACKS_END - USERMODE_STACKS_START) / (CONFIG_STACK_PAGES * PAGE_SIZE)) 

struct process {
	unsigned magic;
	struct vmm_context vmm_context;
	pid_t pid;
	int flags;
	int refs;

	struct llistnode listnode;

	addr_t heap_start, heap_end;
	char command[128];
	char **argv, **env;
	int cmask;
	int tty;
	struct exit_status exit_reason;
	uid_t effective_uid, real_uid, saved_uid;
	gid_t effective_gid, real_gid, saved_gid;
	struct inode *root, *cwd;
	mutex_t files_lock;
	struct file_ptr *filp[FILP_HASH_LEN];
	struct llist mappings;
	mutex_t map_lock;
	struct valloc mmf_valloc;

	/* time accounting */
	time_t utime, stime, cutime, cstime;

	sigset_t global_sig_mask;
	struct sigaction signal_act[128]; /* TODO: only need 32 of these */
	struct process *parent;
	int thread_count;
	struct llist threadlist;
	unsigned char stack_bitmap[NUM_USERMODE_STACKS / 8];
	mutex_t stacks_lock;
};

#define WNOHANG 1

#define CLONE_SHARE_PROCESS 1 /*TODO: these need to match with userland.. */
#define tm_fork() tm_do_fork(0)

extern size_t running_processes;

int tm_set_gid(int n);
int tm_set_uid(int n);
int tm_set_euid(int n);
int tm_set_egid(int n);
int tm_get_gid();
int tm_get_uid();
int tm_get_egid();
int tm_get_euid();

/* TODO: move to CPU subsys */
void arch_tm_set_kernel_stack(struct cpu*, addr_t, addr_t);
void tm_set_kernel_stack(struct cpu*, addr_t, addr_t);

int sys_times(struct tms *buf);
int sys_waitpid(int pid, int *st, int opt);
int sys_wait3(int *, int, int *);

struct process *tm_process_get(pid_t pid);
void tm_process_inc_reference(struct process *proc);
void tm_process_put(struct process *proc);
pid_t sys_getppid();
int sys_alarm(int a);
int sys_gsetpriority(int set, int which, int id, int val);
int sys_waitagain(); /* TODO: clean up any declarations that don't exist anymore */
int sys_nice(int which, int who, int val, int flags);
int sys_setsid();
int sys_setpgid(int a, int b);
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
	struct timeval *timeout);
int sys_sbrk(long inc);
int sys_isstate(pid_t pid, int state);
void sys_exit(int);
int sys_clone(int);

/* provided by arch-dep code */
extern void arch_do_switch_to_user_mode();
void arch_tm_set_current_thread_marker(addr_t *space, addr_t task);
extern struct llist *process_list;

extern struct hash_table *process_table;

#include <sea/mm/vmm.h>

int tm_process_wait(pid_t pid, int state);
pid_t sys_get_pid(void);

int sys_task_stat(unsigned int num, struct task_stat *s);
int sys_task_pstat(pid_t pid, struct task_stat *s);
#define MMF_VMEM_NUM_INDEX_PAGES 12

#endif
