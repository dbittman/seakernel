#include <kernel.h>
#include <memory.h>
#include <elf-x86_64.h>
extern addr_t i_stack;
/* This function's design is based off of JamesM's tutorials. 
 * Yes, I know its bad. But it works okay. */
void move_stack(void *start, size_t sz)
{
	u64int i;
	for(i=((u64int)start)+sz; i >= (u64int)start - sz*2;i -= 0x1000) {
		vm_map(i, pm_alloc_page(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
		memset((void *)i, 0, 0x1000);
	}
	addr_t pd_addr;
	addr_t old_stack_pointer;
	addr_t old_base_pointer;
	addr_t offset = (addr_t)start - i_stack;
	addr_t new_base_pointer, new_stack_pointer, tmp, *tmp2;
	asm("mov %%cr3, %0" : "=r" (pd_addr));
	asm("mov %0, %%cr3" : : "r" (pd_addr)); 
	asm("mov %%rsp, %0" : "=r" (old_stack_pointer));
	asm("mov %%rbp, %0" : "=r" (old_base_pointer));
	new_stack_pointer = old_stack_pointer + offset;
	new_base_pointer  = old_base_pointer  + offset; 
	memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, i_stack-old_stack_pointer);
	
	for(i = (u64int)start; i > (u64int)start-sz; i -= 8)
	{
		tmp = *(u64int*)i;
		if((old_stack_pointer < tmp) && (tmp < i_stack))
		{
			tmp = tmp + offset;
			tmp2 = (u64int*)i;
			*tmp2 = tmp;
		}
	}
	asm("mov %0, %%rsp" : : "r" (new_stack_pointer));
	asm("mov %0, %%rbp" : : "r" (new_base_pointer));
}

void setup_kernelstack()
{
	printk(1, "[stack]: Relocating stack\n");
	move_stack((void*)STACK_LOCATION, STACK_SIZE);
}

void print_trace(unsigned int MaxFrames)
{
	/*
	unsigned int * ebp = &MaxFrames - 2;
	for(unsigned int frame = 0; frame < MaxFrames; ++frame)
	{
		if((kernel_state_flags&KSF_MMU) && !vm_do_getmap((addr_t)ebp, 0, 1)) break;
		unsigned int eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (unsigned int *)(ebp[0]);
		const char *name = elf64_lookup_symbol(eip, &kernel_elf);
		if(name) kprintf("  <%x>  %s\n", eip, name);
	}
	*/
#warning "print_trace"
}

void copy_update_stack(addr_t new, addr_t old, unsigned length)
{
	memcpy((void *)new, (void *)old, length);
	u64int offset=0;
	offset = new-old;
	addr_t i;
	for(i = (addr_t)new+(length-8); i >= (u32int)new; i -= 8)
	{
		u64int tmp = * (u64int*)i;
		if (tmp >= old && tmp < old+length)
		{
			tmp = tmp + offset;
			u64int *tmp2 = (u64int*)i;
			*tmp2 = tmp;
		}
	}
}
