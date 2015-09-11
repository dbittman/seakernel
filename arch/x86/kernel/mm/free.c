/* free.c: Copyright (c) 2010 Daniel Bittman
 * Handles freeing an address space */
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/mm/kmalloc.h>

__attribute__ ((noinline)) static void self_free_table(int t)
{
	addr_t virt = t*1024*PAGE_SIZE;
	int i;
	for(i=0;i<1024;++i)
	{
		if(virt < current_thread->kernel_stack || virt >= (current_thread->kernel_stack + KERN_STACK_SIZE)) {
			if(page_tables[(virt&PAGE_MASK)/PAGE_SIZE])
				mm_vm_unmap(virt, 0);
		}
		virt += PAGE_SIZE;
	}
}

void arch_mm_free_self_directory(int exiting)
{
	unsigned int *pd = (unsigned *)current_process->vmm_context.root_virtual;
	int D = exiting ? PAGE_DIR_IDX(TOP_TASK_MEM/PAGE_SIZE) 
		: PAGE_DIR_IDX(TOP_TASK_MEM_EXEC/PAGE_SIZE);
	int kms_start = PAGE_DIR_IDX(KERNELMODE_STACKS_START / PAGE_SIZE);
	int kms_end = PAGE_DIR_IDX(KERNELMODE_STACKS_END / PAGE_SIZE);
	int i=0;
	for(i=id_tables;i<D;++i)
	{
		if(!pd[i])
			continue;
		if(i >= kms_start && i < kms_end)
			continue; //TODO: does this leak memory?
		self_free_table(i);
		mm_free_physical_page(pd[i]&PAGE_MASK);
		pd[i]=0;
	}
}

void arch_mm_destroy_directory(struct vmm_context *vc)
{
	/* Free the self-ref'ing page table */
	unsigned int *pd = (unsigned *)vc->root_virtual;
	mm_free_physical_page(pd[1022] & PAGE_MASK);
	kfree(pd);
}

