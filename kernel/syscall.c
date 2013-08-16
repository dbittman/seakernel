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
#include <cpu.h>
#include <syscall.h>
unsigned int num_syscalls=0;
//#define SC_DEBUG 1
int sys_null(long a, long b, long c, long d, long e)
{
	#if DEBUG
	kprintf("[kernel]: Null system call (%d) called in task %d\n%x %x %x %x %x", 
			current_task->system, current_task->pid, a, b, c, d, e);
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
	
	SC exit,           SC do_fork,        SC wait_task,     SC sys_readpos, 
	SC sys_writepos,   SC sys_open_posix, SC sys_close,     SC sys_fstat,
	SC sys_stat,       SC sys_isatty,     SC sys_seek,      SC send_signal, 
	SC sys_sbrk,       SC times,          SC sys_dup,       SC sys_dup2,
	
	SC sys_ioctl,      SC sys_null,       SC sys_null,      SC sys_null, 
	SC sys_null,       SC sys_null,       SC sys_null,      SC sys_null,
	SC sys_null,       SC execve, 
	
	#if CONFIG_MODULES
	SC sys_load_module, 
	SC sys_unload_module, 
	SC canweunload, 
	SC unload_all_modules, 
	#else
	SC sys_null,
	SC sys_null,
	SC sys_null,
	SC sys_null,
	#endif
	
	SC get_pid,        SC /**32*/sys_getppid,
	
	SC sys_link,       SC unlink,         SC get_ref_count, SC get_pwd, 
	SC sys_getpath,    SC sys_null,       SC chroot,        SC chdir,
	SC sys_mount,      SC unmount,        SC read_dir,      SC sys_create, 
	SC create_console, SC switch_console, SC sys_null,      SC sys_null,
	
	SC sys_null,       SC sys_mmap,       SC sys_munmap,    SC sys_sync, 
	SC rmdir,          SC sys_fsync,      SC sys_alarm,     SC sys_select,
	SC sys_null,       SC sys_null,       SC sys_sysconf,   SC sys_setsid, 
	SC sys_setpgid, 
	
	#if CONFIG_SWAP
	SC sys_swapon, 
	SC sys_swapoff, 
	#else
	SC sys_null,
	SC sys_null,
	#endif
	
	SC /**64*/sys_nice,
	
	SC sys_null,       SC sys_null,       SC sys_null,      SC task_stat, 
	SC sys_null,       SC sys_null,       SC delay,         SC kernel_reset,
	SC kernel_poweroff,SC get_uid,        SC get_gid,       SC set_uid, 
	SC set_gid,        SC pm_stat_mem,    SC task_pstat,    SC sys_mount2,
	
	SC sys_null,       SC sys_null,       SC sys_pipe,      SC set_signal, 
	SC sys_null,       SC sys_null,       SC sys_null,      SC sys_null,
	SC get_time,       SC get_timer_th,   SC sys_isstate,   SC sys_wait3, 
	SC sys_null,       SC sys_null,       SC sys_getcwdlen, 
	
	#if CONFIG_SWAP
	SC /**96*/sys_swaptask,
	#else
	SC /**96*/sys_null,
	#endif
	
	SC sys_dirstat,    SC sys_sigact,     SC sys_access,    SC sys_chmod, 
	SC sys_fcntl,      SC sys_dirstat_fd, SC sys_getdepth,  SC sys_waitpid,
	SC sys_mknod,      SC sys_symlink,    SC sys_readlink,  SC sys_umask, 
	SC sys_sigprocmask,SC sys_ftruncate,  SC sys_getnodestr,SC sys_chown,
	
	SC sys_utime,      SC sys_gethostname,SC sys_gsetpriority,SC sys_uname, 
	SC sys_gethost,    SC sys_getserv,    SC sys_setserv,     SC sys_syslog,
	SC sys_posix_fsstat,SC sys_null,      SC sys_null,        SC sys_null, 
	SC sys_null,       SC sys_null,       SC sys_waitagain,   SC /**128*/sys_null /* RESERVED*/,
};

void init_syscalls()
{
	num_syscalls = sizeof(syscall_table)/sizeof(void *);
}

static inline int __is_valid_user_ptr(int num, void *p, char flags)
{
	addr_t addr = (addr_t)p;
	if(!addr && !flags) return 0;
	if(addr < TOP_LOWER_KERNEL && addr) {
		#if DEBUG
		printk(5, "[kernel]: warning - task %d passed ptr %x to syscall %d (invalid)\n", 
			   current_task->pid, addr, num);
		#endif
		return 0;
	}
	if(addr >= TOP_TASK_MEM) {
		#if DEBUG
		printk(5, "[kernel]: warning - task %d passed ptr %x to syscall %d (invalid)\n", 
			   current_task->pid, addr, num);
		#endif
		return 0;
	}
	return 1;
}

/* here we test to make sure that the task passed in valid pointers
 * to syscalls that have pointers are arguments, so that we make sure
 * we only ever modify user-space data when we think we're modifying
 * user-space data. */
int check_pointers(volatile registers_t *regs)
{
	switch(SYSCALL_NUM_AND_RET) {
		case SYS_READ: case SYS_FSTAT: case SYS_STAT: case SYS_GETPATH:
		case SYS_READLINK: case SYS_GETNODESTR: 
		case SYS_POSFSSTAT:
			return __is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_B_, 0);
			
		case SYS_TIMES: case SYS_GETPWD: case SYS_PIPE: 
		case SYS_MEMSTAT: case SYS_GETTIME: case SYS_GETHOSTNAME:
		case SYS_UNAME:
			return __is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_A_, 0);
			
		case SYS_SETSIG: case SYS_WAITPID:
			return __is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_B_, 1);
			
		case SYS_SELECT:
			if(!__is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_B_, 1))
				return 0;
			if(!__is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_C_, 1))
				return 0;
			if(!__is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_D_, 1))
				return 0;
			if(!__is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_E_, 1))
				return 0;
			break;
			
		case SYS_DIRSTAT:
			if(!__is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_C_, 0))
				return 0;
			if(!__is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_D_, 0))
				return 0;
			break;
			
		case SYS_SIGACT: case SYS_SIGPROCMASK:
			return __is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_C_, 1);
			
		case SYS_CHOWN: case SYS_CHMOD:
			return __is_valid_user_ptr(SYSCALL_NUM_AND_RET, (void *)_A_, 1);
	}
	return 1;
}

int syscall_handler(volatile registers_t *regs)
{
	/* SYSCALL_NUM_AND_RET is defined to be the correct register in the syscall regs struct. */
	if(unlikely(SYSCALL_NUM_AND_RET >= num_syscalls))
		return -ENOSYS;
	if(unlikely(!syscall_table[SYSCALL_NUM_AND_RET]))
		return -ENOSYS;
	volatile long ret;
	if(!check_pointers(regs))
		return -EINVAL;
	//if(got_signal(current_task) || (unsigned)(ticks-current_task->slice) > (unsigned)current_task->cur_ts)
	//	schedule();
	enter_system(SYSCALL_NUM_AND_RET);
	/* most syscalls are re-entrant, so we enable interrupts and
	 * expect handlers to disable them if needed */
	set_int(1);
	/* start accounting information! */
	current_task->freed = current_task->allocated=0;
	
	#ifdef SC_DEBUG
	if(current_task->tty == curcons->tty) 
		printk(SC_DEBUG, "syscall %d: enter %d\n", current_task->pid, SYSCALL_NUM_AND_RET);
	int or_t = ticks;
	#endif
	__do_syscall_jump(ret, syscall_table[SYSCALL_NUM_AND_RET], _E_, _D_, 
					  _C_, _B_, _A_);
	#ifdef SC_DEBUG
	if(current_task->tty == curcons->tty && (ticks - or_t >= 10 || 1) 
		&& (ret < 0 || 1) && (ret == -EINTR || 1))
		printk(SC_DEBUG, "syscall %d: %d ret %d, took %d ticks\n", 
			   current_task->pid, current_task->system, ret, ticks - or_t);
		#endif
		
	set_int(0);
	exit_system();
	__engage_idle();
	/* if we need to reschedule, or we have overused our timeslice
	 * then we need to reschedule. this prevents tasks that do a continuous call
	 * to write() from starving the resources of other tasks. syscall_count resets
	 * on each call to schedule() */
	if(current_task->flags & TF_SCHED 
		|| (unsigned)(ticks-current_task->slice) > (unsigned)current_task->cur_ts
		|| ++current_task->syscall_count > 2)
	{
		/* clear out the flag. Either way in the if statement, we've rescheduled. */
		current_task->flags &= ~TF_SCHED;
		schedule();
	}
	/* store the return value in the regs */
	SYSCALL_NUM_AND_RET = ret;
	/* if we're going to jump to a signal here, we need to back up our 
	 * return value so that when we return to the system we have the
	 * original systemcall returning the proper value.
	 */
	if((current_task->flags & TF_INSIG) && (current_task->flags & TF_JUMPIN)) {
#if CONFIG_ARCH == TYPE_ARCH_X86
		current_task->reg_b.eax=ret;
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
		current_task->reg_b.rax=ret;
#endif
		current_task->flags &= ~TF_JUMPIN;
	}
	return ret;
}
