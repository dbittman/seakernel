#ifndef TASK_H
#define TASK_H
#include <kernel.h>
#include <memory.h>
#include <fs.h>
#include <sys/stat.h>
#include <sig.h>
#include <mmfile.h>
#include <dev.h>
#include <tqueue.h>
#include <atomic.h>

extern tqueue_t *primary_queue;

#define KERN_STACK_SIZE 0x16000

#define __EXIT     0
#define __COREDUMP 1
#define __EXITSIG  2
#define __STOPSIG  4

#define TASK_MAGIC 0xCAFEBABE
#define FILP_HASH_LEN 512

/* flags for different task states */
#define TF_EXITING     0x2 /* entering the exit() function */
#define TF_ALARM       0x4 /* we have an alarm we are waiting for */
#define TF_SWAP       0x10 /* swapped out */
#define TF_KTASK      0x20 /* this is a kernel process */
#define TF_SWAPQUEUE  0x40 /* waiting to swap */
#define TF_LOCK       0x80 /* locked. the scheduler will not change out of this task */
#define TF_DYING     0x200 /* waiting to be reaped */
#define TF_FORK      0x400 /* newly forked, but hasn't been run yet */
#define TF_INSIG     0x800 /* inside a signal */
#define TF_SCHED    0x1000 /* we request a reschedule after this syscall completes */
#define TF_JUMPIN   0x2000 /* going to jump to a signal handler on the next IRET
						   * used by the syscall handler to determine if it needs to back up
						   * it's return value */
#define TF_LAST_PDIR 0x4000 /* this is the last task referencing it's page directory.
							* this is used to tell the kernel to free the page directory
							* when it cleans up this task */
#define TF_SETINT    0x8000 /* was schedule called with interrupts enabled? if so, 
							 * we need to re-enable them when we schedule into this
							 * task */
#define TF_BURIED   0x10000
#define TF_MOVECPU  0x20000 /* something is trying to move this task to
							 * a different CPU */
#define TF_IN_INT   0x40000 /* inside an interrupt handler */
#define TF_BGROUND  0x80000
#define TF_OTHERBS 0x100000
#define TF_SHUTDOWN  0x200000
#define TF_KILLREADY 0x400000

#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

#define TSEARCH_FINDALL           0x1
#define TSEARCH_PID               0x2
#define TSEARCH_UID               0x4
#define TSEARCH_EUID              0x8
#define TSEARCH_TTY              0x10
#define TSEARCH_PARENT           0x20
#define TSEARCH_ENUM             0x40
#define TSEARCH_EXIT_WAITING     0x80
#define TSEARCH_EXIT_PARENT     0x100
#define TSEARCH_EXCLUSIVE       0x200
#define TSEARCH_ENUM_ALIVE_ONLY 0x400
#define current_tss (&((cpu_t *)current_task->cpu)->tss)

#if SCHED_TTY
static int sched_tty = SCHED_TTY_CYC;
#else
static int sched_tty = 0;
#endif

typedef struct exit_status {
	unsigned pid;
	int ret;
	unsigned sig;
	char coredump;
	int cause;
	struct exit_status *next, *prev;
} ex_stat;

struct thread_shared_data {
	unsigned count;
	mutex_t files_lock;
	struct inode *root, *pwd;
	uid_t uid, _uid;
	gid_t gid, _gid;
	struct sigaction signal_act[128];
	volatile sigset_t global_sig_mask;
	struct file_ptr *filp[FILP_HASH_LEN];
};

struct task_struct
{
	volatile unsigned magic;
	volatile unsigned pid;
	/* used for storing context */
	volatile addr_t eip, ebp, esp;
	page_dir_t *pd;
	/* current state of the task (see sig.h) */
	volatile int state;
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
	mmf_t *mm_files;
	vma_t *mmf_priv_space, *mmf_share_space;
	
	/* signal handling */
	volatile sigset_t sig_mask;
	volatile unsigned sigd, cursig, syscall_count;
	sigset_t old_mask;
	unsigned alarm_end;
	struct llistnode *listnode, *blocknode, *activenode;
	struct llist *blocklist;
	void *cpu;
	struct thread_shared_data *thread;
	volatile struct task_struct *parent, *waiting, *alarm_next, *alarm_prev;
};
typedef volatile struct task_struct task_t;

extern volatile task_t *kernel_task, *tokill, *alarm_list_start;
extern mutex_t *alarm_mutex;

#define raise_task_flag(t,f) or_atomic(&(t->flags), f)
#define lower_task_flag(t,f) and_atomic(&(t->flags), ~f)

#define raise_flag(f) or_atomic(&(current_task->flags), f)
#define lower_flag(f) and_atomic(&(current_task->flags), ~f)

#define WNOHANG 1

#define FORK_SHAREDIR 0x1
#define FORK_SHAREDAT 0x2
#define fork() do_fork(0)

void destroy_task_page_directory(task_t *p);
struct thread_shared_data *thread_data_create();
task_t *task_create();
void move_task_to_kill_queue(task_t *t, int);
task_t *search_tqueue(tqueue_t *tq, unsigned flags, unsigned long value, void (*action)(task_t *, int), int arg, int *);
void delay_sleep(int t);
void take_issue_with_current_task();
void clear_resources(task_t *);
int times(struct tms *buf);
void run_scheduler();
void arch_specific_set_current_task(page_dir_t *, addr_t);
extern volatile long ticks;
int set_gid(int);
int set_uid(int);
void release_mutexes(task_t *t);
int get_gid();
int get_uid();
int do_fork(unsigned);
addr_t read_eip();
int get_exit_status(int pid, int *status, int *retval, int *signum, int *);
int sys_waitpid(int pid, int *st, int opt);
int sys_wait3(int *, int, int *);
int task_stat(unsigned int pid, struct task_stat *s);
int task_pstat(unsigned int, struct task_stat *s);
int schedule();
int get_pid();
void init_multitasking();
void exit(int);
task_t *get_next_task();
void kill_task(unsigned int);
int sys_getppid();
void release_task(task_t *p);
int wait_task(unsigned pid, int state);
void delay(int);
int send_signal(int, int);
void handle_signal(task_t *t);
struct file *get_file_pointer(task_t *t, int n);
void remove_file_pointer(task_t *t, int n);
int add_file_pointer(task_t *t, struct file *f);
int add_file_pointer_after(task_t *, struct file *f, int after);
void copy_file_handles(task_t *p, task_t *n);
void freeze_all_tasks();
void unfreeze_all_tasks();
int sys_alarm(int a);
int get_mem_usage();
void take_issue_with_current_task();
extern volatile unsigned next_pid;
task_t *get_task_pid(int pid);
int do_send_signal(int, int, int);
void kill_all_tasks();
void task_unlock_mutexes(task_t *t);
void do_force_nolock(task_t *t);
void clear_mmfiles(task_t *t, int);
void copy_mmf(task_t *old, task_t *new);
void check_mmf_and_flush(task_t *t, int fd);
int load_so_library(char *name);
int sys_ret_sig();
void close_all_files(task_t *);
int sys_gsetpriority(int set, int which, int id, int val);
int sys_waitagain();
void force_nolock(task_t *);
int get_task_mem_usage(task_t *t);
int sys_nice(int which, int who, int val, int flags);
int sys_setsid();
int sys_setpgid(int a, int b);
extern unsigned init_pid;
void task_suicide();
extern unsigned ret_values_size;
extern unsigned *ret_values;
void set_signal(int sig, addr_t hand);
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
	struct timeval *timeout);
int swap_in_page(task_t *, unsigned);
void task_block(struct llist *list, task_t *task);
void task_pause(task_t *t);
void task_unblock_all(struct llist *list);
void task_unblock(struct llist *list, task_t *t);
void task_resume(task_t *t);
struct inode *set_as_kernel_task(char *name);
void fput(task_t *, int, char);
extern int current_hz;
extern struct llist *kill_queue;
extern unsigned running_processes;
extern void do_switch_to_user_mode();
extern void check_alarms();
static inline int signal_will_be_fatal(task_t *t, int sig)
{
	if(sig == SIGKILL) return 1;
	if(t->thread->signal_act[t->sigd]._sa_func._sa_handler) return 0;
	if(sig == SIGUSLEEP || sig == SIGISLEEP || sig == SIGSTOP || sig == SIGCHILD)
		return 0;
	return 1;
}

static inline int got_signal(task_t *t)
{
	if(kernel_state_flags & KSF_SHUTDOWN)
		return 0;
	if(!t->sigd) return 0;
	/* if the SA_RESTART flag is set, then return false */
	if(t->thread->signal_act[t->sigd].sa_flags & SA_RESTART) return 0;
	/* otherwise, return if we have a signal */
	return (t->sigd);
}

static __attribute__((always_inline)) inline void enter_system(int sys)
{
	current_task->system=(!sys ? -1 : sys);
	current_task->cur_ts/=2;
}

static __attribute__((always_inline)) inline void exit_system()
{
	current_task->last = current_task->system;
	current_task->system=0;
}

static inline int GET_MAX_TS(task_t *t)
{
	if(t->flags & TF_EXITING)
		return 1;
	int x = t->priority;
	if(t->tty == curcons->tty)
		x += sched_tty;
	return x;
}

static inline void __engage_idle()
{
	task_resume((task_t *)kernel_task);
}

static inline void __disengage_idle()
{
	task_pause((task_t *)kernel_task);
}

__attribute__((always_inline)) inline static int task_is_runable(task_t *task)
{
	assert(task);
	if(task->state == TASK_DEAD)
		return 0;
	return (int)(task->state == TASK_RUNNING 
		|| task->state == TASK_SUICIDAL 
		|| (task->state == TASK_ISLEEP && (task->sigd)));
}

#endif
