/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/cpu-x86.h>
#include <sea/mm/kmalloc.h>

_Static_assert(0x400000 % KERN_STACK_SIZE == 0, "0x400000 must be a multiple of KERN_STACK_SIZE");

/* Accepts virtual, returns virtual */
static int vm_do_copy_table(int i, page_dir_t *new, page_dir_t *from, char cow, struct vmm_context *ctx)
{
	addr_t *table;
	addr_t table_phys;
	table = (addr_t *)VIRT_TEMP;
	table_phys = mm_alloc_physical_page();
	cpu_disable_preemption();
	mm_vm_map((addr_t)table, table_phys, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	memset((void *)table, 0, PAGE_SIZE);
	addr_t virt = i*PAGE_SIZE*1024;
	addr_t phyz;
	unsigned attrib;
	int q;
	addr_t init_stack_page = 0;
	for(q=0;virt<((addr_t)((i+1)*PAGE_SIZE*1024));virt+=PAGE_SIZE, ++q)
	{
		if(mm_vm_get_map(virt, &phyz, 1) && mm_vm_get_attrib(virt, &attrib, 1) && virt != VIRT_TEMP)
		{
			/* OK, this page exists */
			if(attrib & PAGE_LINK) {
				/* but we're only going to link it.
				 * WARNING: Features that use PAGE_LINK must be VERY CAREFUL to mm_vm_unmap_only that
				 * page BEFORE the address space is freed normally, since that function DOES NOT KNOW
				 * that multiple mappings may use that physical page! This can lead to memory leaks
				 * and/or duplicate pages in the page stack!!! */
				table[q] = (addr_t)(phyz | attrib);
			} else {
				/* copy like normal */
				addr_t page = mm_alloc_physical_page();
				mm_copy_page_physical((addr_t)phyz /* Source */, (addr_t)page /* Destination*/);
				table[q] = (addr_t)(page | attrib);
			}
		}
	}
	new[i] = table_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	mm_vm_unmap_only((addr_t)table, 1);
	cpu_enable_preemption();
	return 0;
}

static void setup_kernelstack(struct vmm_context *ctx, struct thread *initthread)
{
	struct valloc_region reg;
	valloc_allocate(&ctx->km_stacks, &reg, 1);
	ctx->initial_stack = reg.start;
	addr_t end = reg.start + KERN_STACK_SIZE - PAGE_SIZE;
	addr_t *table = (addr_t *)VIRT_TEMP, table_phys = mm_alloc_physical_page();
	addr_t last_page, first_page;

	int kd = PAGE_DIR_IDX(end / 0x1000);
	addr_t *root = (void *)ctx->root_virtual;
	root[kd] = table_phys | PAGE_PRESENT | PAGE_WRITE;
	mm_vm_map((addr_t)table, table_phys, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	int i=0;
	for(addr_t a = reg.start;a <= end;a+=PAGE_SIZE) {
		addr_t page = mm_alloc_physical_page();
		initthread->kernel_stack_physical[i++] = page;
		if(a == reg.start)
			first_page = page;
		last_page = page;
		int te = PAGE_TABLE_IDX(a / 0x1000);
		table[te] = page | PAGE_PRESENT | PAGE_WRITE;
	}
	mm_vm_unmap_only((addr_t)table, 1);
	mm_vm_map(VIRT_TEMP, last_page, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	addr_t last = VIRT_TEMP;

	if(current_thread->regs) {
		current_thread->regs->eax = 0;
		memcpy((void *)(last + PAGE_SIZE - sizeof(*current_thread->regs)), (void *)current_thread->regs, sizeof(*current_thread->regs));
	}
	ctx->initial_sp = end + PAGE_SIZE - sizeof(*current_thread->regs);

	mm_vm_unmap_only(last, 1);
	mm_vm_map(VIRT_TEMP, first_page, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	addr_t first = VIRT_TEMP;
	*(void **)(first) = initthread;
	mm_vm_unmap_only(first, 1);
}

/* If is it normal task memory or the stack, we copy the tables. Otherwise we simply link them.
 */
static int vm_copy_dir(page_dir_t *from, page_dir_t *new, char flags, struct vmm_context *ctx)
{
	int i=0;
	int D = PAGE_DIR_IDX(TOP_TASK_MEM/PAGE_SIZE);
	int stack = PAGE_DIR_IDX(TOP_TASK_MEM_EXEC/PAGE_SIZE);
	int F = PAGE_DIR_IDX(PDIR_INFO_START/PAGE_SIZE);
	for(i=0;i<F;++i)
	{
		if(!from[i])
			continue; /* TODO: maybe don't copy kernel stacks? */
		/* link if:
		 * 	* Above D
		 *  * below id_tables
		 *  * we request only linking
		 * BUT ONLY IF we're not in the stack memory
		 */
		if((i < id_tables) || (i >= D))
			new[i] = from[i];
		else
			vm_do_copy_table(i, new, from, flags, ctx);
	}
	return 0;
}

void arch_mm_vm_clone(struct vmm_context *oldcontext, struct vmm_context *newcontext, struct thread *initthread)
{
	/* Create new directory and copy it */
	addr_t new_p;
	page_dir_t *new = (page_dir_t *)kmalloc_ap(PAGE_SIZE, &new_p);
	valloc_create(&newcontext->km_stacks, KERNELMODE_STACKS_START, KERNELMODE_STACKS_END, KERN_STACK_SIZE, 0);
	if(pd_cur_data)
		mutex_acquire(&pd_cur_data->lock);


	vm_copy_dir((addr_t *)oldcontext->root_virtual, new, 0, newcontext);
	/* Now set the self refs (DIR_PHYS, TBL_PHYS) */
	new[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	addr_t *tmp = (addr_t *)VIRT_TEMP;
	addr_t tmp_p = mm_alloc_physical_page();
	cpu_disable_preemption();
	mm_vm_map((unsigned)tmp, tmp_p, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_PDLOCKED);
	memset(tmp, 0, PAGE_SIZE);
	tmp[1023] = new_p | PAGE_PRESENT | PAGE_WRITE;
	new[1022] = tmp_p | PAGE_PRESENT | PAGE_WRITE;
	mm_vm_unmap_only((unsigned)tmp, 1);
	cpu_enable_preemption();

	newcontext->root_virtual = (addr_t)new;
	newcontext->root_physical = new_p;
	newcontext->magic = CONTEXT_MAGIC;
	if(initthread)
		setup_kernelstack(newcontext, initthread);
	if(pd_cur_data)
		mutex_release(&pd_cur_data->lock);
	/* TODO: audit these locks */
	mutex_create(&newcontext->lock, MT_NOSCHED);
}

