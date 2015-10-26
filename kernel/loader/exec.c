/* Contains functions for exec'ing files */
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/boot/init.h>
#include <sea/sys/fcntl.h>
#include <sea/cpu/processor.h>
#include <sea/loader/elf.h>
#include <sea/fs/file.h>
#include <sea/fs/file.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>

#include <sea/mm/map.h>
#include <sea/loader/exec.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
/* Prepares a process to recieve a new executable. Desc is the descriptor of 
 * the executable. We keep it open through here so that we dont have to 
 * re-open it. */
void arch_loader_exec_initializer(unsigned argc, addr_t eip);
#undef EXEC_LOG
#define EXEC_LOG 1

static void preexec(int desc)
{
	struct thread *t = current_thread;
	addr_t sysgate_page = mm_physical_allocate(PAGE_SIZE, true);
	mm_physical_memcpy((void *)sysgate_page,
				(void *)signal_return_injector, MEMMAP_SYSGATE_ADDRESS_SIZE, PHYS_MEMCPY_MODE_DEST);
	/* unmap all mappings, specified by POSIX */
	mm_destroy_all_mappings(t->process);
	mm_free_userspace();
	mm_virtual_map(MEMMAP_SYSGATE_ADDRESS, sysgate_page, PAGE_PRESENT | PAGE_USER, PAGE_SIZE);
	/* we need to re-create the vmem for memory mappings */
	valloc_create(&(t->process->mmf_valloc), MEMMAP_MMAP_BEGIN, MEMMAP_MMAP_END, PAGE_SIZE, 0);
	addr_t ret = mm_mmap(t->usermode_stack_start, CONFIG_STACK_PAGES * PAGE_SIZE,
			PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
	mm_page_fault_test_mappings(t->usermode_stack_end - PAGE_SIZE, PF_CAUSE_USER | PF_CAUSE_WRITE);
	t->signal = t->signals_pending = 0;
	memset((void *)t->process->signal_act, 0, sizeof(struct sigaction) * NUM_SIGNALS);

	if(t->flags & THREAD_PTRACED) {
		tm_signal_send_thread(t, SIGTRAP);
	}
}

static void free_dp(char **mem, int num)
{
	/* an error occured and free need to kfree some things */
	int i;
	for(i=0;i<num;i++)
		kfree(mem[i]);
	kfree(mem);
}

static int __is_shebang(char *mem)
{
	return (mem[0] == '#' && mem[1] == '!');
}

int do_exec(char *path, char **argv, char **env, int shebanged /* oh my */)
{
	unsigned int i=0;
	addr_t end, eip;
	unsigned int argc=0, envc=0;
	int desc;
	char **backup_argv=0, **backup_env=0;
	/* Sanity */
	if(!path || !*path)
		return -EINVAL;
	/* Load the file, and make sure that it is valid and accessible */
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Checking executable file (%s)\n", current_process->pid, path);
	struct file *efil;
	int err_open, num;
	efil=fs_do_sys_open(path, O_RDONLY, 0, &err_open, &num);
	if(efil)
		desc = num;
	else
		desc = err_open;
	if(desc < 0 || !efil)
		return -ENOENT;
	/* are we allowed to execute it? */
	if(!vfs_inode_check_permissions(efil->inode, MAY_EXEC, 0))
	{
		sys_close(desc);
		return -EACCES;
	}
	/* is it a valid elf? */
	int header_size = 0;
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	header_size = sizeof(elf64_header_t);
#elif CONFIG_ARCH == TYPE_ARCH_X86
	header_size = sizeof(elf32_header_t);
#endif
	/* read in the ELF header, and check if it's a shebang */
	if(header_size < 2) header_size = 2;
	char mem[header_size];
	fs_read_file_data(desc, mem, 0, header_size);
	
	if(__is_shebang(mem))
		return loader_do_shebang(desc, argv, env);
	
	int other_bitsize=0;
	if(!is_valid_elf(mem, 2) && !other_bitsize) {
		sys_close(desc);
		return -ENOEXEC;
	}
	
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Copy data\n", current_process->pid);
	/* okay, lets back up argv and env so that we can
	 * clear out the address space and not lose data...
	 * If this call if coming from a shebang, then we don't check the pointers,
	 * since they won't be from userspace */
	size_t total_args_len = 0;
	if((shebanged || mm_is_valid_user_pointer(SYS_EXECVE, argv, 0)) && argv) {
		while((shebanged || mm_is_valid_user_pointer(SYS_EXECVE, argv[argc], 0)) && argv[argc] && *argv[argc])
			argc++;
		backup_argv = (char **)kmalloc(sizeof(addr_t) * argc);
		for(i=0;i<argc;i++) {
			backup_argv[i] = (char *)kmalloc(strlen(argv[i]) + 1);
			_strcpy(backup_argv[i], argv[i]);
			total_args_len += strlen(argv[i])+1 + sizeof(char *);
		}
	}
	if((shebanged || mm_is_valid_user_pointer(SYS_EXECVE, env, 0)) && env) {
		while((shebanged || mm_is_valid_user_pointer(SYS_EXECVE, env[envc], 0)) && env[envc] && *env[envc]) envc++;
		backup_env = (char **)kmalloc(sizeof(addr_t) * envc);
		for(i=0;i<envc;i++) {
			backup_env[i] = (char *)kmalloc(strlen(env[i]) + 1);
			_strcpy(backup_env[i], env[i]);
			total_args_len += strlen(env[i])+1 + sizeof(char *);
		}
	}
	total_args_len += 2 * sizeof(char *);
	/* and the path too! */
	char *path_backup = (char *)kmalloc(strlen(path) + 1);
	_strcpy((char *)path_backup, path);
	path = path_backup;
	
	/* Preexec - This is the point of no return. Here we close out unneeded 
	 * file descs, free up the page directory and clear up the resources 
	 * of the task */
	if(EXEC_LOG)
		printk(0, "Executing (p%dt%d, cpu %d, tty %d): %s\n", current_process->pid, current_thread->tid, current_thread->cpu->knum, current_process->tty, path);
	preexec(desc);
	
	/* load in the new image */
	strncpy((char *)current_process->command, path, 128);
	if(!loader_parse_elf_executable(mem, desc, &eip, &end))
		eip=0;
	/* do setuid and setgid */
	if(efil->inode->mode & S_ISUID) {
		current_process->effective_uid = efil->inode->uid;
	}
	if(efil->inode->mode & S_ISGID) {
		current_process->effective_gid = efil->inode->gid;
	}
	/* we don't need the file anymore, close it out */
	sys_close(desc);
	if(!eip) {
		printk(5, "[exec]: Tried to execute an invalid ELF file!\n");
		free_dp(backup_argv, argc);
		free_dp(backup_env, envc);
		kfree(path);
		tm_thread_exit(0);
	}
	
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Updating task values\n", current_process->pid);
	/* Setup the task with the proper values (libc malloc stack) */
	addr_t end_l = end;
	end = ((end-1)&PAGE_MASK) + PAGE_SIZE;
	total_args_len += PAGE_SIZE;
	/* now we need to copy back the args and env into userspace
	 * writeable memory...yippie. */
	addr_t args_start = end + PAGE_SIZE;
	addr_t env_start = args_start;
	addr_t alen = 0;
	mm_mmap(end, total_args_len,
			PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
	if(backup_argv) {
		memcpy((void *)args_start, backup_argv, sizeof(addr_t) * argc);
		alen += sizeof(addr_t) * argc;
		*(addr_t *)(args_start + alen) = 0; /* set last argument value to zero */
		alen += sizeof(addr_t);
		argv = (char **)args_start;
		for(i=0;i<argc;i++)
		{
			char *old = argv[i];
			char *new = (char *)(args_start+alen);
			unsigned len = strlen(old) + 4;
			argv[i] = new;
			_strcpy(new, old);
			kfree(old);
			alen += len;
		}
		kfree(backup_argv);
	}
	env_start = args_start + alen;
	alen = 0;
	if(backup_env) {
		memcpy((void *)env_start, backup_env, sizeof(addr_t) * envc);
		alen += sizeof(addr_t) * envc;
		*(addr_t *)(env_start + alen) = 0; /* set last argument value to zero */
		alen += sizeof(addr_t);
		env = (char **)env_start;
		for(i=0;i<envc;i++)
		{
			char *old = env[i];
			char *new = (char *)(env_start+alen);
			unsigned len = strlen(old) + 1;
			env[i] = new;
			_strcpy(new, old);
			kfree(old);
			alen += len;
		}
		kfree(backup_env);
	}
	end = (env_start + alen) & PAGE_MASK;
	current_process->env = env;
	current_process->argv = argv;
	kfree(path);
	
	/* set the heap locations, and map in the start */
	current_process->heap_start = current_process->heap_end = end + PAGE_SIZE*2;
	addr_t ret = mm_mmap(end + PAGE_SIZE, PAGE_SIZE,
			PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
	/* now, we just need to deal with the syscall return stuff. When the syscall
	 * returns, it'll just jump into the entry point of the new process */
	tm_thread_lower_flag(current_thread, THREAD_SCHEDULE);
	/* the kernel cares if it has executed something or not */
	if(!(kernel_state_flags & KSF_HAVEEXECED))
		set_ksf(KSF_HAVEEXECED);
	arch_loader_exec_initializer(argc, eip);
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Performing call\n", current_process->pid);
	return 0;
}

int execve(char *path, char **argv, char **env)
{
	return do_exec(path, argv, env, 0);
}

