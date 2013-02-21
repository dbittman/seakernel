/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <swap.h>

/* Accepts virtual, returns virtual */
int vm_do_copy_table(int i, page_dir_t *new, page_dir_t *from, char cow)
{
	unsigned *table;
	unsigned table_phys;
	table = (unsigned *)VIRT_TEMP;
	table_phys = pm_alloc_page();
	vm_map((unsigned)table, table_phys, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
	flush_pd();
	memset((void *)table, 0, PAGE_SIZE);
	unsigned virt = i*PAGE_SIZE*1024;
	unsigned phyz;
	unsigned attrib;
	int q;
	for(q=0;virt<((unsigned)((i+1)*PAGE_SIZE*1024));virt+=PAGE_SIZE, ++q)
	{
		if(vm_getmap(virt, &phyz) && vm_getattrib(virt, &attrib))
		{
			/* OK, this page exists, we have the physical address of it too */
			unsigned page = pm_alloc_page();
			copy_page_physical((unsigned)phyz /* Source */, (unsigned)page /* Destination*/);
			table[q] = (unsigned)(page | attrib);
		}
	}
	new[i] = table_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	vm_unmap_only((unsigned)table);
	return 0;
}

/* If is it normal task memory or the stack, we copy the tables. Otherwise we simply link them.
 */
int vm_copy_dir(page_dir_t *from, page_dir_t *new, char cow)
{
	int i=0;
	int D = PAGE_DIR_IDX(TOP_TASK_MEM/PAGE_SIZE);
	int F = PAGE_DIR_IDX(PDIR_INFO_START/PAGE_SIZE);
	for(i=0;i<F;++i)
	{
		if(!from[i])
			continue;
		if((i < id_tables) || (i >= D))
			new[i] = from[i];
		else
			vm_do_copy_table(i, new, from, cow);
	}
	return 0;
}

/* Accepts virtual, returns virtual */
page_dir_t *vm_clone(page_dir_t *pd, char cow)
{
	/* Create new directory and copy it */
#if CONFIG_SWAP
	if(current_task && current_task->num_swapped)
		swap_in_all_the_pages(current_task);
#endif
	unsigned int new_p;
	page_dir_t *new = (page_dir_t *)kmalloc_ap(PAGE_SIZE, &new_p);
	vm_copy_dir(pd, new, cow);
	/* Now set the self refs (DIR_PHYS, TBL_PHYS) */
	new[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	unsigned *tmp = (unsigned *)VIRT_TEMP;
	unsigned tmp_p = pm_alloc_page();
	vm_map((unsigned)tmp, tmp_p, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
	flush_pd();
	tmp[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	new[1022] = tmp_p | PAGE_PRESENT | PAGE_WRITE;
	vm_unmap_only((unsigned)tmp);
	/* map in a page for accounting */
	tmp_p = pm_alloc_page();
	vm_map((unsigned)tmp, tmp_p, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
	flush_pd();
	unsigned pda = tmp[0] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	vm_unmap_only((unsigned)tmp);
	vm_map((unsigned)tmp, pda, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
	memset(tmp, 0, PAGE_SIZE);
	struct pd_data *info = (struct pd_data *)tmp;
	info->count=1;
	/* create the lock. We assume that the only time that two threads
	 * may be trying to access this lock at the same time is when they're
	 * running on different processors, thus we get away with NOSCHED. Also, 
	 * calling schedule() may be problematic inside code that is locked by
	 * this, but it may not be an issue. We'll see. */
	mutex_create(&info->lock, MT_NOSCHED);
	vm_unmap_only((unsigned)tmp);
	new[PAGE_DIR_IDX(PDIR_DATA/PAGE_SIZE)] = tmp_p | PAGE_PRESENT | PAGE_WRITE;	
	return new;
}

page_dir_t *vm_copy(page_dir_t *pd)
{
	/* all this function does is increase the count of 
	 * the current page directory and return it. */
	mutex_acquire(&pd_cur_data->lock);
	pd_cur_data->count++;
	mutex_release(&pd_cur_data->lock);
	return pd;
}
