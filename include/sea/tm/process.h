#ifndef _SEA_TM_PROCESS_H
#define _SEA_TM_PROCESS_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/cpu/registers.h>
#include <sea/tm/signal.h>
#include <sea/mm/context.h>
#include <sea/sys/stat.h>
#include <sea/kernel.h>
#include <sea/mm/vmem.h>
#include <sea/mm/valloc.h>

#define KERN_STACK_SIZE 0x16000

#define FILP_HASH_LEN 512

/* exit reasons */
#define __EXIT     0
#define __COREDUMP 1
#define __EXITSIG  2
#define __STOPSIG  4

#define TASK_MAGIC 0xCAFEBABE
#define THREAD_MAGIC 0xBABECAFE

/* flags for different task states */
#define TF_EXITING        0x2 /* entering the exit() function */
#define TF_ALARM          0x4 /* we have an alarm we are waiting for */
#define TF_SWAP          0x10 /* swapped out */
#define TF_KTASK         0x20 /* this is a kernel process */
#define TF_SWAPQUEUE     0x40 /* waiting to swap */
#define TF_LOCK          0x80 /* locked. the scheduler will not change out of this task */
#define TF_DYING        0x200 /* waiting to be reaped */
#define TF_FORK         0x400 /* newly forked, but hasn't been run yet */
#define TF_INSIG        0x800 /* inside a signal */
#define TF_SCHED       0x1000 /* we request a reschedule after this syscall completes */
#define TF_JUMPIN      0x2000 /* going to jump to a signal handler on the next IRET
* used by the syscall handler to determine if it needs to back up
* it's return value */
#define TF_LAST_PDIR   0x4000 /* this is the last task referencing it's page directory.
* this is used to tell the kernel to free the page directory
* when it cleans up this task */
#define TF_SETINT      0x8000 /* was schedule called with interrupts enabled? if so, 
* we need to re-enable them when we schedule into this
* task */
#define TF_BURIED     0x10000
#define TF_MOVECPU    0x20000 /* something is trying to move this task to
* a different CPU */
#define TF_IN_INT     0x40000 /* inside an interrupt handler */
#define TF_BGROUND    0x80000 /* is waiting for things in the kernel (NOT blocking for a resource) */
#define TF_OTHERBS   0x100000 /* other bit-size. Task is running as different bit-size than the CPU */
#define TF_SHUTDOWN  0x200000 /* this task called shutdown */
#define TF_KILLREADY 0x400000 /* task is ready to be killed */


#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

#define TASK_RUNNING 0
#define TASK_ISLEEP 1
#define TASK_USLEEP 2
#define TASK_SUICIDAL 4
#define TASK_DEAD (-1)

typedef struct exit_status {
	unsigned pid;
	int ret;
	unsigned sig;
	char coredump;
	int cause;
	struct exit_status *next, *prev;
} ex_stat;

struct thread_shared_data {
	unsigned magic;
	unsigned count;
	mutex_t files_lock;
	struct inode *root, *pwd;
	uid_t real_uid, effective_uid, saved_uid;
	gid_t real_gid, effective_gid, saved_gid;
	struct sigaction signal_act[128];
	volatile sigset_t global_sig_mask;
	struct file_ptr *filp[FILP_HASH_LEN];

	struct llist mappings;
	mutex_t map_lock;
	//vma_t mmf_vmem;
	struct valloc mmf_valloc;
};

typedef struct __cpu_t__ cpu_t;

struct task_struct
{
	volatile unsigned magic;
	volatile unsigned pid;
	/* used for storing context */
	volatile addr_t eip, ebp, esp, preserved[16];
	/* this field is required to be aligned on a 16 byte boundary
	 * but since we dynamically allocate task_structs, we cannot
	 * make sure that that will happen. Thus, we need to align it
	 * ourselves... */
	vmm_context_t *pd;
	/* current state of the task (see sig.h) */
	volatile int state;
	char fpu_save_data[512 + 16 /* alignment */];
	volatile unsigned flags;
	int flag;
	
	/* current system call (-1 for the kernel) */
	unsigned int system; 
	addr_t kernel_stack;
	/* timeslicing */
	int cur_ts, priority;
	/* waiting on something? */
	volatile addr_t waiting_ret;
	volatile long tick;
	
	/* accounting */
	time_t stime, utime;
	time_t t_cutime, t_cstime;
	volatile addr_t stack_end;
	volatile unsigned num_pages;
	unsigned last; /* the previous systemcall */
	ex_stat exit_reason, we_res;
	/* pushed registers by the interrupt handlers */
	registers_t reg_b; /* backup */
	registers_t *regs, *sysregs;
	unsigned phys_mem_usage;
	unsigned freed, allocated;
	volatile unsigned wait_again, path_loc_start;
	unsigned num_swapped;
	
	/* executable accounting */
	/*** TODO: So, should these be shared by threads, or no? ***/
	volatile addr_t heap_start, heap_end, he_red;
	char command[128];
	char **argv, **env;
	int cmask;
	int tty;
	unsigned long slice;
	//mmf_t *mm_files;
	//vma_t *mmf_priv_space, *mmf_share_space;
	
	/* signal handling */
	volatile sigset_t sig_mask;
	volatile unsigned sigd, cursig, syscall_count;
	sigset_t old_mask;
	unsigned alarm_end;
	struct llistnode *listnode, *blocknode, *activenode;
	struct llist *blocklist;
	cpu_t *cpu;
	struct thread_shared_data *thread;
	volatile struct task_struct *parent, *waiting, *alarm_next, *alarm_prev;
};
typedef volatile struct task_struct task_t;

extern volatile task_t *kernel_task, *tokill;
extern volatile task_t *alarm_list_start;
extern mutex_t *alarm_mutex;

#define tm_process_raise_flag(t,f) or_atomic(&(t->flags), f)
#define tm_process_lower_flag(t,f) and_atomic(&(t->flags), ~f)

#define tm_raise_flag(f) or_atomic(&(current_task->flags), f)
#define tm_lower_flag(f) and_atomic(&(current_task->flags), ~f)

#define WNOHANG 1

#define FORK_SHAREDIR 0x1
#define FORK_SHAREDAT 0x2
#define tm_fork() tm_do_fork(0)

extern volatile unsigned next_pid;
extern unsigned init_pid;
extern unsigned running_processes;

void tm_process_pause(task_t *t);
void tm_process_resume(task_t *t);
void tm_process_enter_system(int sys);
void tm_process_exit_system();
int tm_do_fork(unsigned flags);
void tm_engage_idle();
task_t *tm_get_process_by_pid(int);

void tm_add_to_blocklist_and_block(struct llist *list, task_t *task);
void tm_add_to_blocklist(struct llist *list, task_t *task);
void tm_remove_from_blocklist(struct llist *list, task_t *t);
void tm_remove_all_from_blocklist(struct llist *list);
void tm_switch_to_user_mode();

int tm_process_got_signal(task_t *t);
int tm_signal_will_be_fatal(task_t *t, int sig);
void tm_set_signal(int sig, addr_t hand);
int sys_get_timer_th(int *t);
void tm_process_suicide();
void tm_kill_process(unsigned int pid);
int tm_process_wait(unsigned pid, int state);
int sys_get_pid();
int sys_task_pstat(unsigned int pid, struct task_stat *s);
int sys_task_stat(unsigned int num, struct task_stat *s);

void tm_exit(int code);
void tm_delay(int t);
void tm_delay_sleep(int t);

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

#define tm_read_eip arch_tm_read_eip
addr_t arch_tm_read_eip();

task_t *tm_task_create();
struct thread_shared_data *tm_thread_data_create();

/* provided by arch-dep code */
extern void arch_do_switch_to_user_mode();
void arch_tm_set_current_task_marker(addr_t *space, addr_t task);

#include <sea/mm/vmm.h>

#define MMF_VMEM_NUM_INDEX_PAGES 12

#endif
