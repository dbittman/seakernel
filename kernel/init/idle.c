/* This file handles the kernel task. It has a couple of purposes: 
 * -> Provides a task to run if all other tasks have slept
 * -> Cleans up after tasks that exit
 * -> Reaps the cache
 * Note: We want to spend as little time here as possible, since it's 
 * cleanup code can run slowly and when theres
 * nothing else to do. So we reschedule often.
 */
#include <kernel.h>
#include <multiboot.h>
#include <console.h>
#include <memory.h>
#include <asm/system.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include <init.h>
#include <mod.h>
#include <cache.h>
#include <swap.h>
void get_timed(struct tm *now);
int __KT_try_releasing_tasks();
extern char *stuff_to_pass[128];
extern char cleared_args;
extern int argc_STP;
extern char tmp_cmd_line[2048];
extern int init_pid;

volatile unsigned int __allow_idle=1;
struct inode *kproclist;
extern struct inode *procfs_kprocdir;
static inline void __KT_clear_args()
{
	/* Clear out the alloc'ed arguments */
	if(!cleared_args && next_pid > (unsigned)(init_pid+1) && init_pid)
	{
		printk(1, "[idle]: Clearing kernel arguments...\n");
		int w=0;
		for(;w<128;w++)
		{
			if(stuff_to_pass[w] && (unsigned)stuff_to_pass[w] > KMALLOC_ADDR_START)
				kfree(stuff_to_pass[w]);
		}
		cleared_args=1;
	}
}

struct inode *set_as_kernel_task(char *name)
{
	printk(1, "[kernel]: Added '%s' as kernel task\n", name);
	struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
	rwlock_create(&i->rwl);
	strncpy(i->name, name, INAME_LEN);
	add_inode(kproclist, i);
	current_task->flags |= TF_KTASK;
	strncpy((char *)current_task->command, name, INAME_LEN);
	return i;
}

int init_kern_task()
{
	kproclist = (struct inode *)kmalloc(sizeof(struct inode));
	_strcpy(kproclist->name, "kproclist");
	kproclist->mode = S_IFDIR | 0xFFF;
	kproclist->count=1;
	kproclist->dev = 256*3;
	rwlock_create(&kproclist->rwl);
	return 0;
}

int kernel_idle_task()
{
	int task, cache;
	if(!fork())
	{
		set_as_kernel_task("kpager");
		/* This task likes to...fuck about with it's page directory.
		 * So we set it's stack at a global location so it doesn't 
		 * screw up some other task's stack. */
		cli();
		set_kernel_stack(current_task->kernel_stack+(KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
		asm("	mov %0, %%esp; \
			mov %0, %%ebp; \
			"::"r"(current_task->kernel_stack+(KERN_STACK_SIZE-STACK_ELEMENT_SIZE)));
		sti();
		__KT_pager();
	}
	set_as_kernel_task("kidle");
	/* First stage is to wait until we can clear various allocated things
	 * that we wont need anymore */
	while(!cleared_args)
	{
		force_schedule();
		__KT_clear_args();
	}
	/* Now enter the main idle loop, waiting to do periodic cleanup */
	for(;;) {
		task=__KT_try_releasing_tasks();
		if(!task && init_pid) {
			__disengage_idle();
			/* Note that, while we go into a wait here, the scheduler 
			 * may awaken the kernel at any time if its the only runable
			 * task. But it doesn't really matter, we'll just end up 
			 * back here. We also ignore signals */
			wait_flag_except((unsigned *)&__allow_idle, 0);
			__super_sti();
		}
	}
}
