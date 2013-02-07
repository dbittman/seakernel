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
	for(i=0;i<1022;++i)
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
	return new;
}
