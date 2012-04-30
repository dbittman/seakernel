#ifndef INIT_H
#define INIT_H
void init();
void setup_kernelstack();
void install_timer(int);
void init_memory(struct multiboot *);
void init_main_cpu();
void init_syscalls();
int exec_init();
void init_dm();
void kb_install();
void load_initrd(struct multiboot *mb);
void process_initrd();
void init_module_system();
void mount_root(char *path);
void load_all_boot_mods();
void try_releasing_tasks();
u32int read_block(int drive, long long addr, unsigned char *buffer);
extern unsigned int heap_end;
extern char mods_toload[128][128];
extern void load_tables();
#define u_fork() dosyscall(SYS_FORK,0,0,0,0,0)
#define u_exit(x) dosyscall(SYS_EXIT,x,0,0,0,0)
#define u_wait(p, s) dosyscall(SYS_WAIT,p,s,0,0,0)
#define u_execve(p, a, e) dosyscall(SYS_EXECVE,(int)p,(int)a,(int)e,0,0)
#define u_write(a, b) dosyscall(SYS_WRITE, a, (int)b, strlen(b), 0, 0)
#define sys_setup() dosyscall(0,0,0,0,0,0)
extern void switch_to_user_mode();
int kernel_cache_sync_slow(int all);
void get_timed(struct tm *now);
extern void enter_system();
void kernel_task_freer();
int kernel_idle_task();
int init_cache();
void init_serial();
int init_kern_task();
extern int initrd_exist;
extern char shutting_down;
/* Adds an env var to the init_env array */
#define add_init_env(x) init_env[count_ie++] = x;init_env[count_ie]=0

#endif
