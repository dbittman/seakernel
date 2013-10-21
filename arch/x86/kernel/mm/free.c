/* free.c: Copyright (c) 2010 Daniel Bittman
 * Handles freeing an address space */
#include <kernel.h>
#include <memory.h>
#include <task.h>
__attribute__ ((noinline)) static void self_free_table(int t)
{
	addr_t virt = t*1024*PAGE_SIZE;
	int i;
	for(i=0;i<1024;++i)
	{
		if(page_tables[(virt&PAGE_MASK)/PAGE_SIZE])
			vm_unmap(virt);
		virt += PAGE_SIZE;
	}
}

void free_thread_shared_directory()
{
	unsigned int *pd = (unsigned *)current_task->pd;
	int D = PAGE_DIR_IDX(TOP_TASK_MEM_EXEC/PAGE_SIZE);
	int A = PAGE_DIR_IDX(SOLIB_RELOC_START/PAGE_SIZE);
	int B = PAGE_DIR_IDX(SOLIB_RELOC_END/PAGE_SIZE);
	int i=0;
	for(i=id_tables;i<D;++i)
	{
		if(!pd[i])
			continue;
		/* Only clear out shared libraries if we are exiting */
		if(i >= A && i < B && !(current_task->flags & TF_EXITING))
			continue;
		self_free_table(i);
		pm_free_page(pd[i]&PAGE_MASK);
		pd[i]=0;
	}
}

void destroy_task_page_directory(task_t *p)
{
	if(p->flags & TF_LAST_PDIR) {
		/* Free the accounting page table */
		pm_free_page(p->pd[PAGE_DIR_IDX(PDIR_DATA/PAGE_SIZE)] & PAGE_MASK);
	}
	/* Free the self-ref'ing page table */
	pm_free_page(p->pd[1022] & PAGE_MASK);
	kfree(p->pd);
}

void free_thread_specific_directory()
{
	unsigned int *pd = (unsigned *)current_task->pd;
	int T = PAGE_DIR_IDX(TOP_TASK_MEM/PAGE_SIZE);
	int S = PAGE_DIR_IDX(TOP_TASK_MEM_EXEC/PAGE_SIZE);
	int i=0;
	for(i=S;i<T;++i)
	{
		if(!pd[i])
			continue;
		self_free_table(i);
		pm_free_page(pd[i]&PAGE_MASK);
		pd[i]=0;
	}
}
