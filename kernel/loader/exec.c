/* Contains functions for exec'ing files */
#include <kernel.h>
#include <task.h>
#include <memory.h>
#include <fs.h>
#include <dev.h>
#include <elf.h>
#include <mod.h>
#include <init.h>
#include <sys/fcntl.h>

/* Prepares a process to recieve a new executable. Desc is the descriptor of the executable. 
 * We keep it open through here so that we dont have to re-open it. */
task_t *preexec(task_t *t, int desc)
{
	if(t->magic != TASK_MAGIC)
		panic(0, "Invalid task in exec (%d)", t->pid);
	clear_resources(t);
	self_free(0);
	memset((void *)t->sig_queue, 0, sizeof(int) * 128);
	memset((void *)t->signal_act, 0, sizeof(struct sigaction) * 128);
	return 0;
}

/* Copy the arguments and environment into a special area in memory:
 * [\0 | 1 | G | R | G | ->0 | -> First Arg | ]
 * Basically, from 0xB to 0xC we have an area where we can store task info that will hold accross exec.
 * 
 * (arg0) (arg1) (arg2) [->null] [->to arg2] [->to arg1] [->to arg0] *
 * is the way its set up. */
#define MAX_ARGV_LEN 0x8000
#define MAX_NUM_ARGS 0x4000
char **copy_down_dp_give(unsigned _location, char **_dp, unsigned *count, unsigned *new_loc)
{
	if(!_dp || !_location)
		return 0;
	char **dp = _dp;
	unsigned i=0;
	unsigned start;
	unsigned location = _location;
	char **ret=0;
	
	map_if_not_mapped_noclear(location);
	
	if(dp && *dp && **dp) {
		/* Loop through all the strings */
		while(*dp && **dp && location > (TOP_TASK_MEM_EXEC) && (location+MAX_ARGV_LEN) > _location)
		{
			if((unsigned)*dp >= TOP_LOWER_KERNEL) {
				unsigned len = strlen(*dp);
				location -= (len+32);
				map_if_not_mapped_noclear(location);
				/* Copy them in */
				memset((void *)location, 0, len+4);
				memcpy((void *)location, (void *)*dp, len);
				*dp = (char *)location;
			}
			++dp;
			++i;
		}
		*dp=0;
		/* Create the double pointer in this memory area */
		start = location-((i+4)*sizeof(char *));
		map_if_not_mapped_noclear(start);
		memcpy((void *)start, (void *)_dp, (i+1)*sizeof(char *));
		ret = (char **)start;
		if(count)
			*count = i;
		location = start-sizeof(void *);
	} else {
		/* If there are no strings, create a dummy with a null pointer */
		location -= sizeof(void *);
		ret = (char **)location;
		*ret = 0;
	}
	if(new_loc)
		*new_loc = (location - sizeof(void *));
	return ret;
}

int copy_double_pointers(char **_args, char **_env, unsigned *argc)
{
	unsigned location = current_task->path_loc_start;
	if(!location) location = TOP_TASK_MEM;
	if(location < (TOP_TASK_MEM - (MAX_ARGV_LEN*2 + MAX_NUM_ARGS*4*2)))
		location = TOP_TASK_MEM;
	location -= sizeof(void *);
	unsigned tmp=0;
	current_task->argv = copy_down_dp_give(location, _args, argc, &tmp);
	if(tmp) location = tmp;
	current_task->env = copy_down_dp_give(location, _env, 0, &tmp);
	return tmp;
}

int do_exec(task_t *t, char *path, char **argv, char **env)
{
	unsigned int end, i=0, eip;
	unsigned int argc=0;
	int envc=0;
	int desc;
	int err = 0;
	/* Sanity */
	if(!t) panic(PANIC_NOSYNC, "Tried to execute with empty task");
	if(t == kernel_task) panic(0, "Kernel is being executed at the gallows!");
	if(t != current_task)
		panic(0, "I don't know, was really drunk at the time");
	if(t->magic != TASK_MAGIC)
		panic(0, "Invalid task in exec (%d)", t->pid);
	if(!path || !*path) 
	{
		err = -EINVAL;
		goto error;
	}
	/* Load the file, and make sure that it is valid and accessable */
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Checking executable file (%s)\n", t->pid, path);
	struct file *efil;
	int err_open, num;
	efil=d_sys_open(path, O_RDONLY, 0, &err_open, &num);
	if(efil)
		desc = num;
	else
		desc = err_open;
	if(desc < 0 || !efil)
	{
		err = -ENOENT;
		goto error;
	}
	if(!permissions(efil->inode, MAY_EXEC))
	{
		err = -EACCES;
		sys_close(desc);
		goto error;
	}
	/* Detirmine if the file is a valid ELF */
	char mem[sizeof(elf_header_t)];
	read_data(desc, mem, 0, sizeof(elf_header_t));
	if(!is_valid_elf32(mem, 2)) {
		err = -ENOEXEC;
		sys_close(desc);
		goto error;
	}
	
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Copy data\n", t->pid);
	struct inode *exe = efil->inode;
	change_icount(exe, 1);
	unsigned path_loc = copy_double_pointers(argv, env, &argc);
	path_loc -= (strlen(path) + 32);
	map_if_not_mapped_noclear(path_loc);
	strcpy((char *)path_loc, path);
	path = (char *)path_loc;
	t->path_loc_start = path_loc;
	/* Preexec - This is the point of no return. Here we close out unneeded file descs, free up the page directory
	 * and clear up the resources of the task */
	if(EXEC_LOG) 
		printk(0, "Executing (task %d, tty %d): %s\n", t->pid, t->tty, path);
	strncpy(t->command, path, 128);
	preexec(t, desc);
	t->exe = exe;
	if(!process_elf(mem, desc, &eip, &end))
		eip=0;
	sys_close(desc);
	if(!eip) {
		printk(5, "[exec]: Tried to execute an invalid ELF file!\n");
#ifdef DEBUG
		panic(0, "");
#endif
		exit(0);
	}
	
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Updating task values\n", t->pid);
	/* Setup the task with the proper values (libc malloc stack) */
	unsigned end_l = end;
	end = (end&PAGE_MASK);
	map_if_not_mapped_noclear(end);
	t->heap_start = t->heap_end = end + 0x1000;
	map_if_not_mapped_noclear(t->heap_start);
	/* Zero the heap and stack */
	memset((void *)end_l, 0, 0x1000-(end_l%0x1000));
	memset((void *)(end+0x1000), 0, 0x1000);
	memset((void *)(STACK_LOCATION - STACK_SIZE), 0, STACK_SIZE);
	__sync_synchronize();
	/* Release everything */
	current_task->state = TASK_RUNNING;
	force_nolock((task_t *)current_task);
	task_full_uncritical();
	__super_cli();
	if(EXEC_LOG == 2) 
		printk(0, "[%d]: Performing call\n", t->pid);
	if(DO_USER_MODE) /* We are currently inside a system call. We must return to usermode before we call */
	{
		assert(current_task->argv && current_task->env);
		set_kernel_stack(current_task->kernel_stack + (KERN_STACK_SIZE-64));
		__sync_synchronize();
		__asm__ __volatile__ ("mov %0, %%esp;\
			mov %0, %%ebp; \
			push %1; \
			push %2; \
			push %3; \
			call do_switch_to_user_mode; \
			call *%4;\
		"::"r"(STACK_LOCATION-64), "r"(current_task->env), "r"(current_task->argv), "r"(argc), "r"(eip):"memory");
	} else
		panic(0, "This feature is no longer supported");
	exit(0);
	for(;;);
	error:
	if(err) return err;
	return -1234;
}

int execve(char *path, char **argv, char **env)
{
	int ret = do_exec((task_t *)current_task, path, argv, env);
	return ret;
}
