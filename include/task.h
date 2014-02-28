#ifndef TASK_H
#define TASK_H
#include <config.h>
#include <kernel.h>
#include <memory.h>
#include <fs.h>
#include <sys/stat.h>
#include <sig.h>
#include <mmfile.h>
#include <dev.h>
#include <atomic.h>
#include <config.h>
#include <file.h>

#include <sea/tm/process.h>
#include <sea/tm/signal.h>
#include <sea/tm/schedule.h>


void destroy_task_page_directory(task_t *p);
struct thread_shared_data *thread_data_create();
task_t *task_create();
void move_task_to_kill_queue(task_t *t, int);
void delay_sleep(int t);
void take_issue_with_current_task();
void clear_resources(task_t *);
int sys_times(struct tms *buf);
void run_scheduler();
void arch_specific_set_current_task(page_dir_t *, addr_t);
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
void kill_task(unsigned int);
int sys_getppid();
void release_task(task_t *p);
int wait_task(unsigned pid, int state);
void delay(int);
struct file *get_file_pointer(task_t *t, int n);
void remove_file_pointer(task_t *t, int n);
int add_file_pointer(task_t *t, struct file *f);
int add_file_pointer_after(task_t *, struct file *f, int after);
void copy_file_handles(task_t *p, task_t *n);
int sys_alarm(int a);
int get_mem_usage();
void take_issue_with_current_task();
task_t *get_task_pid(int pid);
void kill_all_tasks();
void task_unlock_mutexes(task_t *t);
void do_force_nolock(task_t *t);
void close_all_files(task_t *);
int sys_gsetpriority(int set, int which, int id, int val);
int sys_waitagain();
void force_nolock(task_t *);
int get_task_mem_usage(task_t *t);
int sys_nice(int which, int who, int val, int flags);
int sys_setsid();
int sys_setpgid(int a, int b);
void task_suicide();
void handle_signal(task_t *t);
int signal_will_be_fatal(task_t *t, int sig);
int got_signal(task_t *t);
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
	struct timeval *timeout);
int swap_in_page(task_t *, unsigned);
void task_block(struct llist *list, task_t *task);
void task_almost_block(struct llist *list, task_t *task);
int sys_sbrk(long inc);
int execve(char *path, char **argv, char **env);
void task_pause(task_t *t);
void task_unblock_all(struct llist *list);
void task_unblock(struct llist *list, task_t *t);
void task_resume(task_t *t);
struct inode *set_as_kernel_task(char *name);
void fput(task_t *, int, char);
extern void do_switch_to_user_mode();
extern void check_alarms();

#if CONFIG_SMP
void smp_cpu_task_idle(task_t *me);
#endif

extern volatile unsigned next_pid;
extern unsigned init_pid;
extern unsigned ret_values_size;
extern unsigned *ret_values;
extern int current_hz;
extern struct llist *kill_queue;
extern unsigned running_processes;
extern volatile long ticks;

#endif
