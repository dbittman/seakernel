/* manager.c - Provide swapping management. Keeping track of devices, etc. */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <sea/fs/inode.h>
#include <sea/mm/swap.h>
#include <sea/sys/stat.h>
#include <mod.h>
mutex_t *sl_mutex;
swapdev_t *swaplist=0;
struct inode *get_sb_table(int n);
volatile unsigned num_swapdev=0;
/* Linked list stuff */
void add_swapdevice(swapdev_t *d)
{
	mutex_on(sl_mutex);
	assert(d);
	swapdev_t *tmp = swaplist;
	swaplist=d;
	swaplist->next=tmp;
	if(tmp) tmp->prev = swaplist;
	d->prev=0;
	num_swapdev++;
	mutex_off(sl_mutex);
}

void remove_swapdevice(swapdev_t *d)
{
	mutex_on(sl_mutex);
	if(d->prev)
		d->prev->next=d->next;
	else
	{
		assert(d==swaplist);
		swaplist=d->next;
	}
	if(d->next)
		d->next->prev=d->prev;
	d->prev=d->next=0;
	num_swapdev--;
	mutex_off(sl_mutex);
}

swapdev_t *find_swapdevice(int dev)
{
	swapdev_t *s = swaplist;
	while(s)
	{
		if(s->dev == dev)
			return s;
		s=s->next;
	}
	return 0;
}

void init_swap()
{
	sl_mutex=create_mutex(0);
	swaplist=0;
	num_swapdev=0;
}

swapdev_t *init_swapdevice(int dev, unsigned size, unsigned flags, unsigned bs)
{
	swapdev_t *s = find_swapdevice(dev);
	if(s)
	{
		printk(6, "[swap]: Device %x already registered!\n", dev);
		return 0;
	}
	device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
	{
		printk(6, "[swap]: Device %x does not exist or is not a block device!\n", dev);
		return 0;
	}
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	if(!bd)
		return 0;
#ifdef SWAP_DEBUG
	printk(0, "[swap]: Registering device %x as a swap device\n", dev);
#endif
	s = (swapdev_t *) kmalloc(sizeof(swapdev_t));
	s->dev=dev;
	s->size=size * bs;
	s->flags=flags;
	s->blocksize=bs;
	s->nslots = size / (0x1000 / bs);
	s->pi_size = s->nslots * 12; /* 12 bytes per page info in index */
	s->page_index = (page_index_t *)kmalloc(s->pi_size);
	s->page_index->page = 0xFFFFFFFF;
	s->block_index = (unsigned char *)kmalloc(s->nslots);
	s->lock = create_mutex(0);
#ifdef SWAP_DEBUG
	printk(0, "[swap]: Disabling block cache on device %x\n", dev);
#endif
	s->old_cache = dm_block_ioctl(dev, -2, 0);
	dm_block_ioctl(dev, -3, 0);
	add_swapdevice(s);
#ifdef SWAP_DEBUG
	printk(0, "[swap]: Added swap device\n");
#endif
	return s;
}

int sys_swapon(char *node, unsigned size /*0 for all */)
{
	if(!node)
		return -EINVAL;
	if(current_task->uid) {
		printk(6, "[swap]: Must be root to change swap devices\n");
		return -1;
	}
	if(size)
		panic(PANIC_MEM | PANIC_NOSYNC, "swapon got a non-zero value for size");
	unsigned dev=0;
	struct inode *in = vfs_get_idir(node, 0);
	if(in)
		dev = in->dev;
	if(!dev)
	{
		if(in) vfs_iput(in);
		printk(6, "[swap]: Could not open device %s\n", node);
		return -1;
	}
	int c=0;
	struct inode *i;
	while((i=get_sb_table(c)))
	{
		if((in == i) || (in->dev == i->dev))
		{
			printk(6, "[swap]: Device %s already mounted!\n", node);
			vfs_iput(in);
			return -1;
		}
		c++;
	}
	if(in) vfs_iput(in);
	unsigned bs=0;
	if(!size) {
		size = dm_block_ioctl(dev, -7, (int)&bs);
	}
	else 
		bs=1;
	if(!size)
	{
		printk(6, "[swap]: Could not detirmine size of device %s\n", node);
		return -1;
	}
	printk(2, "[swap]: Swap-on: %s (%x) for %d Bytes\n", node, dev, size);
	swapdev_t *s = init_swapdevice(dev, size, SW_EMPTY | SW_ENABLE, bs);
	if(!s)
		return -1;
	strncpy(s->node, node, 16);
	return 0;
}

int return_paged_memory(swapdev_t *s)
{
	panic(PANIC_MEM | PANIC_NOSYNC, "Return paged memory not implemented");
	return 0;
}

int sys_swapoff(char *node, unsigned flags)
{
	if(!node)
		return -EINVAL;
	if(current_task->uid) {
		printk(6, "[swap]: Must be root to change swap devices\n");
		return -1;
	}
	unsigned dev=0;
	struct inode *in = vfs_get_idir(node, 0);
	if(in) {
		vfs_iput(in);
		dev = in->dev;
	}
	if(!dev)
	{
		printk(6, "[swap]: Could not open device %s\n", node);
		return -1;
	}
	swapdev_t *s = find_swapdevice(dev);
	mutex_on(s->lock);
	if(!(s->flags & SW_EMPTY))
	{
		printk(6, "[swap]: Warning - device %s contains swapped out data!\n", node);
		if(!(flags & SW_FORCE)) {
			printk(6, "[swap]: Aborting\n");
			mutex_off(s->lock);
			return -1;
		}
		printk(6, "[swap]: Returning data to memory...\n");
		s->flags &= ~SW_ENABLE;
		return_paged_memory(s);
	}
	s->flags &= ~SW_ENABLE;
#ifdef SWAP_DEBUG
	printk(0, "[swap]: Swapping on device %s disabled. Restoring cache settings...\n", node);
#endif
	remove_swapdevice(s);
	dm_block_ioctl(dev, -3, s->old_cache);
	kfree(s->page_index);
	kfree(s->block_index);
	destroy_mutex(s->lock);
	kfree(s);
	printk(2, "[swap]: Disabled swap on device %s\n", node);
	return 0;
}

extern unsigned tmp_page;
int do_page_out_task_addr(task_t *t, unsigned addr)
{
	if(!t)
		return 0;
	/* Don't fuck with this tasks memory if it's loading from swap, or if 
	 * something is swapping it */
	if(t->flags & TF_SWAP) 
		return -1;
	char tmp[4096];
	addr &= PAGE_MASK;
	lock_scheduler();
	int oldstate = t->state;
	/* Make sure the task doesn't run til we are finished */
	t->state = TASK_FROZEN;
	/* Switch our directory to that of the task */
	vm_switch(t->pd);
	flush_pd();
	//current_dir = t->pd;
	/* Copy the page data to the global directory */
	unsigned attr;
	if((attr = vm_getattrib(addr, 0)))
		memcpy(tmp, (void *)addr, 0x1000);
	/* Switch back to our directory */
	vm_switch(current_task->pd);
	//current_dir = current_task->pd;
	flush_pd();
	unlock_scheduler();
	if(!attr) {
		t->state = oldstate;
		return -1;
	}
	/* Map it in where it was in theirs */
	unsigned p = tmp_page;
	mm_vm_map(addr, p, attr, 0);
	memcpy((void *)addr, tmp, 0x1000);
	/* And swap out the page */
	if(!swap_page_out(t, addr)) {
		/* Unmaps the above allocation */
		vm_unmap_only(addr);
		/* If that worked, lets unmap it. */
		lock_scheduler();
		vm_switch(t->pd);
		flush_pd();
		//current_dir = t->pd;
		vm_unmap(addr);
		vm_switch(current_task->pd);
		//current_dir = current_task->pd;
		flush_pd();
		unlock_scheduler();
	} else
	{
		/* If it didn't work, we need to unmap and free what we 
		 * allocated just above */
		vm_unmap_only(addr);
	}
	t->state = oldstate;
	return 0;
}

int page_out_task_addr(task_t *t, unsigned addr)
{
	int ret = do_page_out_task_addr(t, addr);
	if(ret == 0)
		printk(1, "[swap]: Swapped out page %x of task %d\n", addr, t->pid);
	return ret;
}

int swap_task(task_t *t)
{
	if(!t)
		return -EINVAL;
	if(!num_swapdev)
		return -1;
	printk(1, "Swapping task %d\n", t->pid);
	int i = id_tables;
	unsigned tot=0;
	int D = PAGE_DIR_IDX(TOP_TASK_MEM/0x1000);
	for(;i<D;i++)
	{
		if(t->pd[i])
		{
			int j;
			for(j=0;j<1024;j++)
			{
				unsigned addr = i*1024*0x1000 + j*0x1000;
				tot += page_out_task_addr(t, addr)+1;
			}
		}
	}
	printk(1, "Done swapping task\n");
	return tot;
}

void __KT_swapper()
{
	
	//for(;;)
	//{
		tm_delay(1000);
		
		if(num_swapdev)
		{
			task_t *p = kernel_task->next;
			while(p)
			{
				if(p->flags & TF_SWAPQUEUE)
				{
					p->flags &= ~TF_SWAPQUEUE;
					unlock_scheduler();
					swap_task(p);
					printk(2, "[swap]: Swapped out %d pages (%d KB) from task %d\n", 
						p->num_swapped, p->num_swapped/4, p->pid);
					lock_scheduler();
				}
				p=p->next;
			}
			/* Swapping algorithm */
			if(((pm_used_pages * 100) / pm_num_pages) > 60)
			{
				task_t *t = kernel_task->next;
				while(t)
				{
					if((t->tty != current_console->tty || 
							(((pm_used_pages * 100) / pm_num_pages) > 80)) || 
							t->state != TASK_RUNNING) {
						if(t->tty) swap_task(t);
						break;
					}
					t=t->next;
					//tm_delay(100);
				}
			}
		}
	//}
}

int sys_swaptask(unsigned pid)
{
	if(current_task->uid)
		return -1;
	task_t *t = tm_get_process_by_pid(pid);
	if(!t)
		return -2;
	if(t->flags & TF_KTASK || !t->pid)
		return -3;
	if(!num_swapdev)
		return -4;
	t->flags |= TF_SWAPQUEUE;
	return 0;
}
