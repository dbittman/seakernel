#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/loader/elf-x86_64.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>

void arch_cpu_print_stack_trace(unsigned int MaxFrames)
{
	addr_t * ebp;
	__asm__ __volatile__ ("mov %%rbp, %0" : "=r"(ebp));
	kprintf("        ADDR          MODULE FUNCTION\n");
	for(unsigned int frame = 0; frame < MaxFrames; ++frame)
	{
		if((kernel_state_flags&KSF_MMU) && !mm_vm_get_map((addr_t)ebp, 0, 1)) break;
		addr_t eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (addr_t *)(ebp[0]);
		const char *name = elf32_lookup_symbol(eip, &kernel_elf);
		char *modname = 0;
		if(!name)
			name = loader_lookup_module_symbol(eip, &modname);
		if(name) kprintf(" <%16.16x> %8s %s\n", eip, modname ? modname : "kernel", name);
	}
}

void arch_cpu_print_stack_trace_alternate(addr_t *ebp)
{
	kprintf("    ADDR      MODULE FUNCTION\n");
	for(int frame = 0; frame < 32; ++frame)
	{
		if((kernel_state_flags&KSF_MMU) && !mm_vm_get_map((addr_t)ebp, 0, 1)) break;
		addr_t eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (addr_t *)(ebp[0]);
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
	u64int offset=0;
	offset = new-old;
	addr_t i;
	for(i = (addr_t)new+(length-8); i >= (addr_t)new; i -= 8)
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
