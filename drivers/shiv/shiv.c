#include <sea/kernel.h>
#include <sea/loader/module.h>
#include <sea/types.h>
#include <sea/cpu/processor.h>
#include <sea/tm/process.h>
#include <sea/cpu/x86msr.h>

#include <modules/shiv.h>

uint32_t revision_id; /* this is actually only 31 bits, but... */
addr_t vmxon_region=0;

int shiv_check_hardware_support()
{
	/* check CPUID.1 ECX.VMX */
	cpu_t *cpu = current_task->cpu;
	if(!(cpu->cpuid.features_ecx & (1 << 5)))
	{
		printk(0, "[shiv]: no support for VMX on this processor\n");
		return 0;
	}
	/* check IA32_FEATURE_CONTROL MSR */
	uint64_t v = read_msr(MSR_IA32_FEATURE_CONTROL);
	if(((v & 1) && !(v & (1 << 2))))
	{
		printk(0, "[shiv]: VMX disabled by BIOS\n");
		return 0;
	}
	/* check for secondary-execution-control support */
	v = read_msr(MSR_IA32_PROCBASED_CTLS);
	if(!(v & ((uint64_t)1 << 63)))
	{
		printk(0, "[shiv]: VMX doesn't support secondary controls\n");
		return 0;
	}
	/* check secondary control features */
	v = read_msr(MSR_IA32_PROCBASED_CTLS2);
	if(!(v & ((uint64_t)1 << (32 + 1)))) 
	{
		printk(0, "[shiv]: VMX doesn't allow EPT\n");
		return 0;
	}
	if(!(v & ((uint64_t)1 << (32 + 7))))
	{
		printk(0, "[shiv]: VMX doesn't allow unrestricted guest\n");
		return 0;
	}
	return 1;
}

int shiv_vmx_on()
{
	uint64_t cr4, v;
	asm("mov %%cr4, %0":"=r"(cr4));
	if(cr4 & (1 << 13))
	{
		printk(0, "[shiv]: VMX operation is already enabled on this CPU\n");
		return 1;
	}
	/* set VMX in non SMX operation enable bit */
	v = read_msr(MSR_IA32_FEATURE_CONTROL);
	if(!(v & 1)) { 
		v |= (1 << 2) | 1;
		write_msr(MSR_IA32_FEATURE_CONTROL, v);
	}
	/* verify */
	v = read_msr(MSR_IA32_FEATURE_CONTROL);
	if(!(v & (1 << 2)))
	{
		printk(0, "[shiv]: could not enable VMX in feature control MSR\n");
		return 0;
	}
	printk(0, "[shiv]: VMX has required features, reading control settings...\n");
	/* allow VMXON */
	cr4 |= (1 << 13);
	asm("mov %0, %%cr4"::"r"(cr4));

	/* read basic configuration info */
	v = read_msr(MSR_IA32_VMX_BASIC);
	revision_id = v & 0x7FFFFFFF;
	if(v & ((uint64_t)1 << 48))
		printk(0, "[shiv]: warning: processor doesn't allow virtualized physical memory beyond 32 bits\n");

	/* enable */
	vmxon_region = mm_alloc_physical_page();
	asm(".byte 0xf3, 0x0f, 0xc7, 0x30"::"a"(&vmxon_region), "m"(vmxon_region):"memory", "cc");
	return 1;
}

int shiv_vmx_off()
{

}

addr_t shiv_alloc_vmx_data_area()
{

}

/* returns physical address */
addr_t shiv_build_ept_pml4(addr_t memsz)
{
	addr_t pml4 = mm_alloc_physical_page();
	for(addr_t i=0;i<memsz;i++)
		arch_mm_vm_early_map(pml4 + PHYS_PAGE_MAP, i, mm_alloc_physical_page(), 7, MAP_NOCLEAR); 
	return pml4;
}

void shiv_vm_exit_handler()
{

}

struct vmcs *shiv_alloc_vmcs()
{

}

void shiv_init_vmcs(struct vmachine *vm)
{

}

int shiv_init_virtual_machine(struct vmachine *vm)
{

}

int shiv_init_vmm()
{

}

int shiv_launch_or_resume(struct vmachine *vm)
{

}

int module_install()
{
	if(!shiv_check_hardware_support())
	{
		printk(4, "[shiv]: CPU doesn't support required features\n");
		return -EINVAL;
	}
	if(!shiv_vmx_on())
	{
		printk(4, "[shiv]: couldn't enable VMX operation\n");
		return -EINVAL;
	}
	return 0;
}

int module_exit()
{
	return 0;
}

