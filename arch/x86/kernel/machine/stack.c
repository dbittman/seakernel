#include <kernel.h>
#include <memory.h>
#include <elf-x86.h>
extern unsigned int i_stack;
/* This function's design is based off of JamesM's tutorials. 
 * Yes, I know its bad. But it works okay. */
void move_stack(void *start, unsigned int sz)
{
	unsigned i;
	for(i=(unsigned int)start+sz; i >= (unsigned int)start - sz*2;i -= 0x1000) {
		vm_map(i, pm_alloc_page(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
		memset((void *)i, 0, 0x1000);
	}
	unsigned pd_addr;
	u32int old_stack_pointer;
	u32int old_base_pointer;
	u32int offset = (u32int)start - i_stack;
	u32int new_base_pointer, new_stack_pointer, tmp, *tmp2;
	asm("mov %%cr3, %0" : "=r" (pd_addr));
	asm("mov %0, %%cr3" : : "r" (pd_addr)); 
	asm("mov %%esp, %0" : "=r" (old_stack_pointer));
	asm("mov %%ebp, %0" : "=r" (old_base_pointer));
	new_stack_pointer = old_stack_pointer + offset;
	new_base_pointer  = old_base_pointer  + offset; 
	memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, i_stack-old_stack_pointer);
	
	for(i = (u32int)start; i > (u32int)start-sz; i -= 4)
	{
		tmp = * (u32int*)i;
		if (( old_stack_pointer < tmp) && (tmp < i_stack))
		{
			tmp = tmp + offset;
			tmp2 = (u32int*)i;
			*tmp2 = tmp;
		}
	}
	asm("mov %0, %%esp" : : "r" (new_stack_pointer));
	asm("mov %0, %%ebp" : : "r" (new_base_pointer));
}

void setup_kernelstack()
{
	printk(1, "[stack]: Relocating stack\n");
	move_stack((void*)STACK_LOCATION, STACK_SIZE);
}

void print_trace(unsigned int MaxFrames)
{
	unsigned int * ebp = &MaxFrames - 2;
	for(unsigned int frame = 0; frame < MaxFrames; ++frame)
	{
		if((kernel_state_flags&KSF_MMU) && !vm_do_getmap((addr_t)ebp, 0, 1)) break;
		unsigned int eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (unsigned int *)(ebp[0]);
		const char *name = elf32_lookup_symbol(eip, &kernel_elf);
		if(name) kprintf("  <%x>  %s\n", eip, name);
	}
}

void copy_update_stack(addr_t new, addr_t old, unsigned length)
{
	memcpy((void *)new, (void *)old, length);
	int offset=0;
	offset = new-old;
	addr_t i;
	for(i = (u32int)new+(length-4); i >= (u32int)new; i -= 4)
	{
		u32int tmp = * (u32int*)i;
		if (tmp >= old && tmp < old+length)
		{
			tmp = tmp + offset;
			u32int *tmp2 = (u32int*)i;
			*tmp2 = tmp;
		}
	}
}
