/* syscall.c: Copyright (c) 2010 Daniel Bittman
* With some design input on the actual syscall interface from JamesM's tutorials
* 
* Provide access to the kernel for applications. */
#include <kernel.h>
#include <syscall.h>
#include <isr.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include <sys/stat.h>
#include <mod.h>
#include <sys/sysconf.h>
#include <swap.h>
unsigned int num_syscalls=0;
//#define SC_DEBUG 1
int sys_null()
{
#ifdef DEBUG
	kprintf("[kernel]: Null system call (%d) called in task %d\n", 
			current_task->system, current_task->pid);
#endif
	return -ENOSYS;
}

int sys_gethost(int a, int b, int c, int d, int e)
{
	/* Remember to add systemcall buffer checks... */
	return -ENOSYS;
}

int sys_getserv(int a, int b, int c, int d, int e)
{
	return -ENOSYS;
}

int sys_setserv(int a, int b, int c, int d, int e)
{
	return -ENOSYS;
}

int sys_syslog(int level, char *buf, int len, int ctl)
{
	if(ctl)
		return 0;
	printk(level, "%s", buf);
	return 0;
}

void *syscall_table[129] = {
	SC sys_setup,
	
	SC exit, SC fork, SC wait_task, SC sys_readpos, 
	SC sys_writepos, SC sys_open_posix, SC sys_close, SC sys_fstat,
	SC sys_stat, SC sys_isatty, SC sys_seek, SC send_signal, 
	SC sys_sbrk, SC times, SC sys_dup, SC sys_dup2,
	
	SC sys_ioctl, SC sys_null, SC dfs_cn, SC remove_dfs_node, 
	SC sys_null, SC sys_null, SC sys_null, SC sys_null,
	SC sys_null, SC execve, SC sys_load_module, SC sys_unload_module, 
	SC canweunload, SC unload_all_modules, SC get_pid, SC /**32*/sys_getppid,
	
	SC sys_link, SC unlink, SC get_ref_count, SC get_pwd, 
	SC sys_getpath, SC sys_null, SC chroot, SC chdir,
	SC sys_mount, SC unmount, SC read_dir, SC sys_create, 
	SC create_console, SC switch_console, SC sys_null, SC sys_null,
	
	SC sys_null, SC sys_mmap, SC sys_munmap, SC sys_sync, 
	SC rmdir, SC sys_fsync, SC sys_alarm, SC sys_select,
	SC sys_null, SC sys_null, SC sys_sysconf, SC sys_setsid, 
	SC sys_setpgid, SC sys_swapon, SC sys_swapoff, SC /**64*/sys_nice,
	
	SC sys_null, SC sys_null, SC sys_null, SC task_stat, 
	SC sys_null, SC sys_null, SC delay, SC kernel_reset,
	SC kernel_poweroff, SC get_uid, SC get_gid, SC set_uid, 
	SC set_gid, SC pm_stat_mem, SC task_pstat, SC sys_mount2,
	
	SC sys_null, SC sys_null, SC sys_pipe, SC set_signal, 
	SC sys_null, SC sys_null, SC sys_null, SC sys_null,
	SC get_time, SC get_timer_th, SC sys_isstate, SC sys_wait3, 
	SC sys_null, SC sys_null, SC sys_getcwdlen, SC /**96*/sys_swaptask,
	
	SC sys_dirstat, SC sys_sigact, SC sys_access, SC sys_chmod, 
	SC sys_fcntl, SC sys_dirstat_fd, SC sys_getdepth, SC sys_waitpid,
	SC sys_mknod, SC sys_symlink, SC sys_readlink, SC sys_umask, 
	SC sys_sigprocmask, SC sys_ftruncate, SC sys_getnodestr, SC sys_chown,
	
	SC sys_utime, SC sys_gethostname, SC sys_gsetpriority, SC sys_uname, 
	SC sys_gethost, SC sys_getserv, SC sys_setserv, SC sys_syslog,
	SC sys_posix_fsstat, SC sys_null, SC sys_null, SC sys_null, 
	SC sys_null, SC sys_null, SC sys_waitagain, SC /**128*/sys_ret_sig,
};

void init_syscalls()
{
	register_interrupt_handler (0x80, (isr_t)&syscall_handler);
	register_interrupt_handler (80, (isr_t)&syscall_handler);
	num_syscalls = sizeof(syscall_table)/sizeof(void *);
}

#define _E_ regs->edi
#define _D_ regs->esi
#define _C_ regs->edx
#define _B_ regs->ecx
#define _A_ regs->ebx
int check_pointers(volatile registers_t *regs)
{
	switch(regs->eax) {
		case SYS_READ: case SYS_FSTAT: case SYS_STAT: case SYS_GETPATH:
		case SYS_READLINK: case SYS_GETNODESTR: 
		case SYS_POSFSSTAT:
			return __is_valid_user_ptr((void *)_B_, 0);
		
		case SYS_TIMES: case SYS_GETPWD: case SYS_PIPE: 
		case SYS_MEMSTAT: case SYS_GETTIME: case SYS_GETHOSTNAME:
		case SYS_UNAME:
			return __is_valid_user_ptr((void *)_A_, 0);
		
		case SYS_SETSIG: case SYS_WAITPID:
			return __is_valid_user_ptr((void *)_B_, 1);
		
		case SYS_SELECT:
			if(!__is_valid_user_ptr((void *)_B_, 1))
				return 0;
			if(!__is_valid_user_ptr((void *)_C_, 1))
				return 0;
			if(!__is_valid_user_ptr((void *)_D_, 1))
				return 0;
			if(!__is_valid_user_ptr((void *)_E_, 1))
				return 0;
			break;
		
		case SYS_DIRSTAT:
			if(!__is_valid_user_ptr((void *)_C_, 0))
				return 0;
			if(!__is_valid_user_ptr((void *)_D_, 0))
				return 0;
			break;
			
		case SYS_SIGACT: case SYS_SIGPROCMASK:
			return __is_valid_user_ptr((void *)_C_, 1);
			
		case SYS_CHOWN: case SYS_CHMOD:
			return __is_valid_user_ptr((void *)_A_, 1);
	}
	return 1;
}

__attribute__((optimize("O0"))) int syscall_handler(volatile registers_t *regs)
{
	if (regs->eax >= num_syscalls)
		return -ENOSYS;
	if(!syscall_table[regs->eax])
		return -ENOSYS;
	volatile int ret;
	__super_sti();
	if(!check_pointers(regs))
		return -EINVAL;
	if(got_signal(current_task))
		force_schedule();
	enter_system(regs->eax);
	current_task->freed = current_task->allocated=0;
#ifdef SC_DEBUG
	if(current_task->tty == curcons->tty) 
		printk(SC_DEBUG, "syscall %d: enter %d\n", current_task->pid, regs->eax);
	int or_t = ticks;
#endif
	__do_syscall_jump(ret, syscall_table[regs->eax], regs->edi, regs->esi, 
			regs->edx, regs->ecx, regs->ebx);
#ifdef SC_DEBUG
	if(current_task->tty == curcons->tty && (ticks - or_t >= 10 || 1) 
			&& (ret < 0||1))
		printk(SC_DEBUG, "syscall %d: %d ret %d, took %d ticks\n", 
				current_task->pid, current_task->system, ret, ticks - or_t);
#endif
	cli();
	exit_system();
	/* store the return value in the regs */
	regs->eax = ret;
	return ret;
}

int dosyscall(int num, int a, int b, int c, int d, int e)
{
	int x;
	asm("int $0x80":"=a"(x):"0" (num), "b" ((int)a), "c" ((int)b), "d" ((int)c), "S" ((int)d), "D" ((int)e));
	return x;
}
