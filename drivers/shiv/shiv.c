#include <sea/loader/module.h>
#include <sea/types.h>
#include <sea/errno.h>
#include <sea/cpu/processor.h>
#include <sea/tm/process.h>
#include <sea/cpu/x86msr.h>
#include <sea/asm/system.h>
#include <sea/mm/vmm.h>

#include <sea/vsprintf.h>

#include <modules/shiv.h>

uint32_t revision_id; /* this is actually only 31 bits, but... */
addr_t vmxon_region=0;

#define __pa(x) (addr_t)mm_vm_get_map((addr_t)x,0,0)

static void vmcs_clear(struct vmcs *vmcs)
{
	uint64_t phys_addr = __pa(vmcs);
	uint8_t error;

	asm (ASM_VMX_VMCLEAR_RAX "; setna %0"
			: "=g"(error) : "a"(&phys_addr), "m"(phys_addr)
			: "cc", "memory");
	if (error)
		printk(4, "vmclear fail: %x/%x\n",
				vmcs, phys_addr);
}

static void vcpu_clear(struct vcpu *vcpu)
{
	vmcs_clear(vcpu->vmcs);
	vcpu->launched = 0;
}

static unsigned long vmcs_readl(unsigned long field)
{
	unsigned long value;

	asm (ASM_VMX_VMREAD_RDX_RAX
			: "=a"(value) : "d"(field) : "cc");
	return value;
}

static uint16_t vmcs_read16(unsigned long field)
{
	return vmcs_readl(field);
}

static uint32_t vmcs_read32(unsigned long field)
{
	return vmcs_readl(field);
}

static uint64_t vmcs_read64(unsigned long field)
{
	return vmcs_readl(field);
}

static void vmwrite_error(unsigned long field, unsigned long value)
{
	printk(2, "vmwrite error: reg %x value %x (err %d)\n",
			field, value, vmcs_read32(VM_INSTRUCTION_ERROR));
}

static void vmcs_writel(unsigned long field, unsigned long value)
{
	uint8_t error;

	asm (ASM_VMX_VMWRITE_RAX_RDX "; setna %0"
			: "=q"(error) : "a"(value), "d"(field) : "cc" );
	if (unlikely(error))
		vmwrite_error(field, value);
}

static void vmcs_write16(unsigned long field, uint16_t value)
{
	vmcs_writel(field, value);
}

static void vmcs_write32(unsigned long field, uint32_t value)
{
	vmcs_writel(field, value);
}

static void vmcs_write64(unsigned long field, uint64_t value)
{
	vmcs_writel(field, value);
}

/*
 *  * Switches to specified vcpu, until a matching vcpu_put(), but assumes
 *   * vcpu mutex is already taken.
 *    */
static void vmx_vcpu_load(cpu_t *cpu, struct vcpu *vcpu)
{
	uint64_t phys_addr = __pa(vcpu->vmcs);

	if(vcpu->cpu != cpu)
		vcpu_clear(vcpu);

	uint8_t error;

	asm (ASM_VMX_VMPTRLD_RAX "; setna %0"
				: "=g"(error) : "a"(&phys_addr), "m"(phys_addr)
				: "cc");
	if (error)
		printk(2, "kvm: vmptrld %x/%x fail\n",
					vcpu->vmcs, phys_addr);

	if (vcpu->cpu != cpu) {
		vcpu->cpu = cpu;
		/* TODO: These lines need to be confirmed */
		vmcs_writel(HOST_TR_BASE, (unsigned long)(&cpu->arch_cpu_data.tss));
		vmcs_writel(HOST_GDTR_BASE, (unsigned long)(cpu->arch_cpu_data.gdt_ptr.base));
	}
}


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
	/* magic code */
	asm(".byte 0xf3, 0x0f, 0xc7, 0x30"::"a"(&vmxon_region), "m"(vmxon_region):"memory", "cc");
	return 1;
}

int shiv_vmx_off()
{

}

/* returns physical address */
addr_t shiv_build_ept_pml4(addr_t memsz)
{
	addr_t pml4 = mm_alloc_physical_page();
	/* hack: limit memory */
	if(memsz > 1023)
		memsz = 1023;
	if(memsz < 255)
		memsz = 255;
	addr_t init_code = mm_alloc_physical_page();
	for(addr_t i=0;i<memsz;i++) {
		if(i == 255) {
			/* map in special init code */
			arch_mm_vm_early_map((void *)(pml4 + PHYS_PAGE_MAP), i, init_code, 7, MAP_NOCLEAR); 
		} else {
			arch_mm_vm_early_map((void *)(pml4 + PHYS_PAGE_MAP), i, mm_alloc_physical_page(), 7, MAP_NOCLEAR); 
		}
	}
	uint8_t *vinit = (void *)(pml4 + PHYS_PAGE_MAP);
	vinit[0x0FFF0] = 0x66;
	vinit[0x0FFF1] = 0xb8;
	vinit[0x0FFF2] = 0x12;
	vinit[0x0FFF3] = 0x00;
	vinit[0x0FFF4] = 0xf4;
	return pml4;
}

void shiv_skip_instruction(struct vcpu *vc)
{

}

void shiv_vm_exit_handler()
{
	/* check cause of exit */

	/* handle exit reasons */

	/* return to VM */
}

struct vmcs *shiv_alloc_vmcs()
{

}

void shiv_init_vmcs(struct vcpu *vm)
{
	/* allocate a vmcs for a cpu by calling shiv_alloc_vmcs */

	/* initialize the region as needed */
}

struct vcpu *shiv_create_vcpu(struct vmachine *vm)
{
	/* create structure */

	/* set up guest CPU state as needed */

	/* return vcpu */
}

int shiv_init_virtual_machine(struct vmachine *vm)
{
	/* set id */
}

int shiv_launch_or_resume(struct vmachine *vm)
{
	/* LOOP */
	/* save host state */

	/* launch / resume */

	/* restore host state */

	/* check cause of exit - 1: was is a launch / resume error?
	 *   if yes, return failure
	 *   if no, call shiv_vm_exit_handler()
	 */
}

struct vmachine *shiv_create_vmachine()
{
	struct vmachine *vm = kmalloc(sizeof(*vm));
	int ret = shiv_init_virtual_machine(vm);
	if(ret == -1) {
		kfree(vm);
		return 0;
	}
	vm->vcpu = shiv_create_vcpu(vm);
	return vm;
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

