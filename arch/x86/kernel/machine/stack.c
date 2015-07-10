#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/loader/elf-x86.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>

void arch_cpu_print_stack_trace(int MaxFrames)
{
	unsigned int * ebp = (unsigned int *)&MaxFrames - 2;
	kprintf("    ADDR      MODULE FUNCTION\n");
	for(int frame = 0; frame < MaxFrames; ++frame)
	{
		if((kernel_state_flags&KSF_MMU) && !mm_vm_get_map((addr_t)ebp, 0, 1)) break;
		unsigned int eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (unsigned int *)(ebp[0]);
		const char *name = elf32_lookup_symbol(eip, &kernel_elf);
		char *modname = 0;
		if(!name)
			name = loader_lookup_module_symbol(eip, &modname);
		if(name) kprintf(" <%8.8x> %8s %s\n", eip, modname ? modname : "kernel", name);
	}
}

void arch_cpu_copy_fixup_stack(addr_t new, addr_t old, size_t length)
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

