#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/loader/elf-x86_64.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>

void arch_cpu_print_stack_trace(int MaxFrames)
{
	addr_t * ebp;
	__asm__ __volatile__ ("mov %%rbp, %0" : "=r"(ebp));
	kprintf("        ADDR          MODULE FUNCTION\n");
	addr_t last_eip = 0;
	for(int frame = 0; frame < MaxFrames; ++frame)
	{
		if((kernel_state_flags&KSF_MMU) && !mm_virtual_getmap((addr_t)ebp, 0, 0)) break;
		addr_t eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (addr_t *)(ebp[0]);
		if(eip == last_eip) {
			kprintf(" .");
		} else {
			const char *name = elf64_lookup_symbol(eip, &kernel_elf);
			char *modname = 0;
			if(!name)
				name = loader_lookup_module_symbol(eip, &modname);
			if(name) kprintf(" <%16.16x> %8s %s\n", eip, modname ? modname : "kernel", name);
		}
		last_eip = eip;
	}
}

void arch_cpu_print_stack_trace_alternate(struct thread *thr, addr_t *ebp)
{
	kprintf("    ADDR      MODULE FUNCTION\n");
	for(int frame = 0; frame < 32; ++frame)
	{
		//if((kernel_state_flags&KSF_MMU) && !mm_vm_get_map((addr_t)ebp, 0, 1)) break;
		addr_t eip;
		if(!mm_context_read(&thr->process->vmm_context, &eip, (addr_t)ebp + 8, 8))
			break;
		if(eip == 0)
			break;
		if(!mm_context_read(&thr->process->vmm_context, &ebp, (addr_t)ebp, 8))
			break;
		const char *name = elf64_lookup_symbol(eip, &kernel_elf);
		char *modname = 0;
		if(!name)
			name = loader_lookup_module_symbol(eip, &modname);
		if(name) kprintf(" <%8.8x> %8s %s\n", eip, modname ? modname : "kernel", name);
	}
}

