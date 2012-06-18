/* free.c: Copyright (c) 2010 Daniel Bittman
 * Handles freeing an address space */
#include <kernel.h>
#include <memory.h>
#include <task.h>
int self_free_table(int t)
{
	unsigned virt = t*1024*PAGE_SIZE;
	int i;
	for(i=0;i<1024;++i)
	{
		if(page_tables[(virt&PAGE_MASK)/PAGE_SIZE])
			vm_unmap(virt);
		virt += PAGE_SIZE;
	}
	return 0;
}

int self_free(int all)
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
	return 0;
}

int free_stack()
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
	return 0;
}
