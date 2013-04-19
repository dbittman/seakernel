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
	vm_map((unsigned)table, table_phys, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	flush_pd();
	memset((void *)table, 0, PAGE_SIZE);
	unsigned virt = i*PAGE_SIZE*1024;
	unsigned phyz;
	unsigned attrib;
	int q;
	for(q=0;virt<((unsigned)((i+1)*PAGE_SIZE*1024));virt+=PAGE_SIZE, ++q)
	{
		if(vm_do_getmap(virt, &phyz, 1) && vm_do_getattrib(virt, &attrib, 1))
		{
			/* OK, this page exists, we have the physical address of it too */
			unsigned page = pm_alloc_page();
			copy_page_physical((unsigned)phyz /* Source */, (unsigned)page /* Destination*/);
			table[q] = (unsigned)(page | attrib);
		}
	}
	new[i] = table_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	vm_do_unmap_only((unsigned)table, 1);
	return 0;
}

/* If is it normal task memory or the stack, we copy the tables. Otherwise we simply link them.
 */
int vm_copy_dir(page_dir_t *from, page_dir_t *new, char flags)
{
	int i=0;
	int D = PAGE_DIR_IDX(TOP_TASK_MEM/PAGE_SIZE);
	int stack = PAGE_DIR_IDX(TOP_TASK_MEM_EXEC/PAGE_SIZE);
	int F = PAGE_DIR_IDX(PDIR_INFO_START/PAGE_SIZE);
	for(i=0;i<F;++i)
	{
		if(!from[i])
			continue;
		/* link if:
		 * 	* Above D
		 *  * below id_tables
		 *  * we request only linking
		 * BUT ONLY IF we're not in the stack memory
		 */
		if(((i < id_tables) || (i >= D) || (flags & 1)) && !(i < D && i >= stack))
			new[i] = from[i];
		else
			vm_do_copy_table(i, new, from, flags);
	}
	return 0;
}
extern unsigned imps_lapic_addr;
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
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	vm_copy_dir(pd, new, cow ? 2 : 0);
	/* Now set the self refs (DIR_PHYS, TBL_PHYS) */
	new[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	unsigned *tmp = (unsigned *)VIRT_TEMP;
	unsigned tmp_p = pm_alloc_page();
	vm_map((unsigned)tmp, tmp_p, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	flush_pd();
	memset(tmp, 0, PAGE_SIZE);
	tmp[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	new[1022] = tmp_p | PAGE_PRESENT | PAGE_WRITE;
	vm_do_unmap_only((unsigned)tmp, 1);
	
#if CONFIG_SMP
	/* we can link since all page directories have this table set up
	 * already */
	unsigned pdi = PAGE_DIR_IDX(imps_lapic_addr / PAGE_SIZE);
	new[pdi] = pd[pdi];
#endif
	
	/* map in a page for accounting */
	tmp_p = pm_alloc_page();
	vm_map((unsigned)tmp, tmp_p, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	flush_pd();
	unsigned pda = tmp[0] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	vm_do_unmap_only((unsigned)tmp, 1);
	vm_map((unsigned)tmp, pda, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	memset(tmp, 0, PAGE_SIZE);
	struct pd_data *info = (struct pd_data *)tmp;
	info->count=1;
	/* create the lock. We assume that the only time that two threads
	 * may be trying to access this lock at the same time is when they're
	 * running on different processors, thus we get away with NOSCHED. Also, 
	 * calling schedule() may be problematic inside code that is locked by
	 * this, but it may not be an issue. We'll see. */
	mutex_create(&info->lock, MT_NOSCHED);
	vm_do_unmap_only((unsigned)tmp, 1);
	new[PAGE_DIR_IDX(PDIR_DATA/PAGE_SIZE)] = tmp_p | PAGE_PRESENT | PAGE_WRITE;
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
	return new;
}

/* this sets up a new page directory in almost exactly the same way:
 * the directory is specific to the thread, but everything inside
 * the directory is just linked to the parent directory. The count
 * on the directory usage is increased, and the accounting page is 
 * linked so it can be accessed by both threads */
page_dir_t *vm_copy(page_dir_t *pd)
{
#if CONFIG_SWAP
	if(current_task && current_task->num_swapped)
		swap_in_all_the_pages(current_task);
#endif
	unsigned int new_p;
	page_dir_t *new = (page_dir_t *)kmalloc_ap(PAGE_SIZE, &new_p);
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	pd_cur_data->count++;
	/* link as much as we can (flag & 1) */
	vm_copy_dir(pd, new, 1);
	/* Now set the self refs (DIR_PHYS, TBL_PHYS) */
	new[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	unsigned *tmp = (unsigned *)VIRT_TEMP;
	unsigned tmp_p = pm_alloc_page();
	vm_map((unsigned)tmp, tmp_p, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	flush_pd();
	tmp[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	new[1022] = tmp_p | PAGE_PRESENT | PAGE_WRITE;
	vm_do_unmap_only((unsigned)tmp, 1);

#if CONFIG_SMP
	/* we can link since all page directories have this table set up
	 * already */
	unsigned pdi = PAGE_DIR_IDX(imps_lapic_addr / PAGE_SIZE);
	new[pdi] = pd[pdi];
#endif

	/* map the current accounting page into the new directory */
	int account = PAGE_DIR_IDX(PDIR_DATA/PAGE_SIZE);
	new[account] = pd[account];
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
	return new;
}
