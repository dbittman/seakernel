/* This file handles the kernel task. It has a couple of purposes: 
 * -> Provides a task to run if all other tasks have slept
 * -> Cleans up after tasks that tm_exit
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
#include <symbol.h>
#include <cache.h>
#include <swap.h>
void get_timed(struct tm *now);
int __KT_try_releasing_tasks();
void __KT_try_handle_stage2_interrupts();

struct inode *kproclist;

static inline int __KT_clear_args()
{
	/* Clear out the alloc'ed arguments */
	if(next_pid > (unsigned)(init_pid+1) && init_pid)
	{
		printk(1, "[idle]: clearing unused kernel memory...\n");
		int w=0;
		for(;w<128;w++)
		{
			if(stuff_to_pass[w] && (addr_t)stuff_to_pass[w] > KMALLOC_ADDR_START)
				kfree(stuff_to_pass[w]);
		}
		return 1;
	}
	return 0;
}

struct inode *kt_set_as_kernel_task(char *name)
{
	struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
	rwlock_create(&i->rwl);
	strncpy(i->name, name, INAME_LEN);
	add_inode(kproclist, i);
	tm_raise_flag(TF_KTASK);
	strncpy((char *)current_task->command, name, 128);
	printk(1, "[kernel]: Added '%s' as kernel task\n", name);
	return i;
}

int kt_init_kernel_tasking()
{
	kproclist = (struct inode *)kmalloc(sizeof(struct inode));
	_strcpy(kproclist->name, "kproclist");
	kproclist->mode = S_IFDIR | 0xFFF;
	kproclist->count=1;
	kproclist->dev = GETDEV(3, 0);
	rwlock_create(&kproclist->rwl);
#if CONFIG_MODULES
	loader_do_add_kernel_symbol((addr_t)(struct inode **)&kproclist, "kproclist");
#endif
	return 0;
}

int kt_kernel_idle_task()
{
	int task, cache;
#if CONFIG_SWAP
	if(!tm_fork())
	{
		kt_set_as_kernel_task("kpager");
		__KT_pager();
	}
#endif
	kt_set_as_kernel_task("kidle");
	/* First stage is to wait until we can clear various allocated things
	 * that we wont need anymore */
	while(!__KT_clear_args())
	{
		tm_schedule();
		set_int(1);
	}
	set_int(0);
	printk(1, "[kernel]: remapping lower memory with protection flags...\n");
	addr_t addr = 0;
	while(addr != TOP_LOWER_KERNEL)
	{
		/* set it to write. We don't actually have to do this, because
		 * ring0 code may always access memory. As long as the PAGE_USER
		 * flag isn't set... */
		if(!(SIGNAL_INJECT >= addr && SIGNAL_INJECT < (addr + PAGE_SIZE_LOWER_KERNEL)))
			vm_setattrib(addr, PAGE_PRESENT | PAGE_WRITE);
		addr += PAGE_SIZE_LOWER_KERNEL;
	}
	set_int(1);
	/* Now enter the main idle loop, waiting to do periodic cleanup */
	printk(0, "[idle]: entering background loop\n");
	for(;;) {
		task=__KT_try_releasing_tasks();
		__KT_try_handle_stage2_interrupts();
		tm_schedule();
		set_int(1);
	}
}
