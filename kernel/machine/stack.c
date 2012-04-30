#include <kernel.h>
#include <memory.h>
extern unsigned int i_stack;
/* This function's design is based off of JamesM's tutorials */
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

void setup_kernelstack(int c)
{
	printk(1, "[stack]: Relocating stack\n");
	move_stack((void*)STACK_LOCATION, STACK_SIZE);
}

void copy_update_stack(unsigned new, unsigned old, unsigned length)
{
	memcpy((void *)new, (void *)old, length);
	int offset=0;
	offset = new-old;
	unsigned i;
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
