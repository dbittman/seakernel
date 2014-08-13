#ifndef INIT_H
#define INIT_H

#include <sea/boot/multiboot.h>
#include <sea/syscall.h>

void init();
void setup_kernelstack();
void install_timer(int);
void init_memory(struct multiboot *);
void init_main_cpu();
void syscall_init();
int exec_init();
void init_dm();
void kb_install();
void load_initrd(struct multiboot *mb);
void process_initrd();
void init_module_system();
void mount_root(char *path);
void load_all_boot_mods();
void try_releasing_tasks();
extern unsigned int heap_end;
extern char mods_toload[128][128];
extern void load_tables();
extern void switch_to_user_mode();
int kernel_cache_sync_slow(int all);
void get_timed(struct tm *now);
extern void enter_system();
void kernel_task_freer();
int kt_kernel_idle_task();
void fs_init();
int init_cache();
int kt_init_kernel_tasking();
void serial_init();
void net_init();
void tm_init_multitasking();
/* Adds an env var to the init_env array */
#define add_init_env(x) init_env[count_ie++] = x;init_env[count_ie]=0

/* system calls that we'll need */
#define u_fork() dosyscall(SYS_FORK,0,0,0,0,0)
#define u_fork2() dosyscall(SYS_FORK,FORK_SHAREDIR,0,0,0,0)
#define u_exit(x) dosyscall(SYS_EXIT,x,0,0,0,0)
#define u_wait(p, s) dosyscall(SYS_WAIT,p,s,0,0,0)
#define u_execve(p, a, e) dosyscall(SYS_EXECVE,(addr_t)p,(addr_t)a,(addr_t)e,0,0)
#define u_write(a, b) dosyscall(SYS_WRITE, a, (addr_t)b, strlen(b), 0, 0)
#define sys_setup() dosyscall(0,0,0,0,0,0)

extern char *stuff_to_pass[128];

#endif
