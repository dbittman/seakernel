#ifndef _SEA_TM_PROCESS_H
#define _SEA_TM_PROCESS_H

#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/lib/linkedlist.h>
#include <sea/cpu/registers.h>
#include <sea/tm/signal.h>
#include <sea/sys/stat.h>
#include <sea/mm/valloc.h>
#include <sea/tm/async_call.h>
#include <sea/lib/hash.h>
#include <sea/tm/thread.h>

#define current_process (current_thread ? current_thread->process : 0)

#define NUM_FD 128

/* exit reasons */
#define __EXIT     0
#define __COREDUMP 1
#define __EXITSIG  2
#define __STOPSIG  4
#define __STOPPED  8

#define PROCESS_MAGIC 0xCAFEBABE

#define PROCESS_CLEANED 1
#define PROCESS_EXITED  2

struct exit_status {
	pid_t pid;
	int ret;
	unsigned sig;
	char coredump;
	int cause;
};

#define NUM_POSSIBLE_USERMODE_STACKS ((MEMMAP_USERSTACKS_END - MEMMAP_USERSTACKS_START) / (CONFIG_STACK_PAGES * PAGE_SIZE)) 

#define NUM_POSSIBLE_KERNELMODE_STACKS ((MEMMAP_KERNELSTACKS_END - MEMMAP_KERNELSTACKS_START) / (KERN_STACK_SIZE)) 

_Static_assert(NUM_POSSIBLE_USERMODE_STACKS <= NUM_POSSIBLE_KERNELMODE_STACKS,
		"memory map wrong, need at least same number of kernel stacks as user stacks per process");

#if NUM_POSSIBLE_USERMODE_STACKS > 256
	#define NUM_USERMODE_STACKS 256
#else
	#define NUM_USERMODE_STACKS NUM_POSSIBLE_USERMODE_STACKS
#endif

#define NUM_SIGNALS 32

#define WIFSTOPPED(w)   (((w) & 0xff) == 0x7f)

struct pty;
struct process {
	unsigned magic;
	struct vmm_context vmm_context;
	pid_t pid;
	int flags;
	_Atomic int refs;

	struct linkedentry listnode;

	addr_t heap_start, heap_end;
	char command[128];
	char **argv, **env;
	int cmask;
	struct exit_status exit_reason;
	uid_t effective_uid, real_uid, saved_uid;
	gid_t effective_gid, real_gid, saved_gid;
	struct inode *root, *cwd;
	struct mutex fdlock;
	struct hash files;
	unsigned char fdnum_bitmap[NUM_FD / 8];
	struct linkedlist mappings;
	struct mutex map_lock;
	struct valloc mmf_valloc;

	/* time accounting */
	time_t utime, stime, cutime, cstime;

	sigset_t global_sig_mask;
	struct sigaction signal_act[NUM_SIGNALS];
	struct process *parent;
	int thread_count;
	struct linkedlist threadlist;
	unsigned char stack_bitmap[NUM_USERMODE_STACKS / 8];
	struct mutex stacks_lock;
	struct blocklist waitlist;
	struct hashelem hash_elem;

	struct pty *pty;
};

#define WNOHANG 1

#define CLONE_SHARE_PROCESS 1
#define CLONE_KTHREAD 0x100
#define tm_fork() tm_do_fork(0)

extern size_t running_processes;
extern struct process *kernel_process;
int tm_set_gid(int n);
int tm_set_uid(int n);
int tm_set_euid(int n);
int tm_set_egid(int n);
int tm_get_gid();
int tm_get_uid();
int tm_get_egid();
int tm_get_euid();

int sys_times(struct tms *buf);
int sys_waitpid(int pid, int *st, int opt);
int sys_wait3(int *, int, int *);
pid_t sys_getppid();
int sys_alarm(int a);
int sys_gsetpriority(int set, int which, int id, int val);
int sys_nice(int which, pid_t who, int val, int flags);
int sys_setsid();
int sys_setpgid(int a, int b);
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
	struct timeval *timeout);
int sys_sbrk(long inc);
int sys_isstate(pid_t pid, int state);

/* provided by arch-dep code */
extern struct linkedlist *process_list;

extern struct hash *process_table;

#include <sea/mm/vmm.h>

pid_t sys_get_pid(void);

int sys_task_stat(unsigned int num, struct task_stat *s);
int sys_task_pstat(pid_t pid, struct task_stat *s);
#define MMF_VMEM_NUM_INDEX_PAGES 12

void tm_process_wait_cleanup(struct process *proc);
struct process *tm_process_get(pid_t pid);
void tm_process_inc_reference(struct process *proc);
void tm_process_put(struct process *proc);
int tm_signal_send_process(struct process *proc, int signal);
int sys_kill(pid_t pid, int signal);
void tm_process_create_kerfs_entries(struct process *proc);
struct thread *tm_process_get_head_thread(struct process *proc);

extern addr_t sysgate_page;

#endif

