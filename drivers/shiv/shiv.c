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

static void vmcs_write32_fixedbits(uint32_t msr, uint32_t vmcs_field, uint32_t val)
{
	uint32_t msr_high, msr_low;

	uint64_t given = read_msr(msr);
	msr_high = given >> 32;
	msr_low = given & 0xFFFFFFFF;

	val &= msr_high;
	val |= msr_low;
	vmcs_write32(vmcs_field, val);
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

/*
 * Sets up the vmcs for emulated real mode.
 */
static int shiv_vcpu_setup(struct vcpu *vcpu)
{
	int i;
	int ret = 0;

	//if (!init_rmode_tss(vcpu->kvm)) {
	//	ret = -ENOMEM;
	//	goto out;
	//}

	memset(vcpu->regs, 0, sizeof(vcpu->regs));
	vcpu->regs[VCPU_REGS_RDX] = get_rdx_init_val();
	vcpu->apic_base = 0xfee00000 |
		/*for vcpu 0*/ MSR_IA32_APICBASE_BSP |
		MSR_IA32_APICBASE_ENABLE;

	//fx_init(vcpu);

	/*
	 * GUEST_CS_BASE should really be 0xffff0000, but VT vm86 mode
	 * insists on having GUEST_CS_BASE == GUEST_CS_SELECTOR << 4.  Sigh.
	 */
	vmcs_write16(GUEST_CS_SELECTOR, 0xf000);
	vmcs_writel(GUEST_CS_BASE, 0x000f0000);
	vmcs_write32(GUEST_CS_LIMIT, 0xffff);
	vmcs_write32(GUEST_CS_AR_BYTES, 0x9b);

	seg_setup(VCPU_SREG_DS);
	seg_setup(VCPU_SREG_ES);
	seg_setup(VCPU_SREG_FS);
	seg_setup(VCPU_SREG_GS);
	seg_setup(VCPU_SREG_SS);

	vmcs_write16(GUEST_TR_SELECTOR, 0);
	vmcs_writel(GUEST_TR_BASE, 0);
	vmcs_write32(GUEST_TR_LIMIT, 0xffff);
	vmcs_write32(GUEST_TR_AR_BYTES, 0x008b);

	vmcs_write16(GUEST_LDTR_SELECTOR, 0);
	vmcs_writel(GUEST_LDTR_BASE, 0);
	vmcs_write32(GUEST_LDTR_LIMIT, 0xffff);
	vmcs_write32(GUEST_LDTR_AR_BYTES, 0x00082);

	vmcs_write32(GUEST_SYSENTER_CS, 0);
	vmcs_writel(GUEST_SYSENTER_ESP, 0);
	vmcs_writel(GUEST_SYSENTER_EIP, 0);

	vmcs_writel(GUEST_RFLAGS, 0x02);
	vmcs_writel(GUEST_RIP, 0xfff0);
	vmcs_writel(GUEST_RSP, 0);

	//todo: dr0 = dr1 = dr2 = dr3 = 0; dr6 = 0xffff0ff0
	vmcs_writel(GUEST_DR7, 0x400);

	vmcs_writel(GUEST_GDTR_BASE, 0);
	vmcs_write32(GUEST_GDTR_LIMIT, 0xffff);

	vmcs_writel(GUEST_IDTR_BASE, 0);
	vmcs_write32(GUEST_IDTR_LIMIT, 0xffff);

	vmcs_write32(GUEST_ACTIVITY_STATE, 0);
	vmcs_write32(GUEST_INTERRUPTIBILITY_INFO, 0);
	vmcs_write32(GUEST_PENDING_DBG_EXCEPTIONS, 0);

	/* I/O */
	vmcs_write64(IO_BITMAP_A, 0);
	vmcs_write64(IO_BITMAP_B, 0);

	guest_write_tsc(0);

	vmcs_write64(VMCS_LINK_POINTER, -1ull); /* 22.3.1.5 */

	/* Special registers */
	vmcs_write64(GUEST_IA32_DEBUGCTL, 0);

	/* Control */
	vmcs_write32_fixedbits(MSR_IA32_VMX_PINBASED_CTLS,
			PIN_BASED_VM_EXEC_CONTROL,
			PIN_BASED_EXT_INTR_MASK   /* 20.6.1 */
			| PIN_BASED_NMI_EXITING   /* 20.6.1 */
			);
	vmcs_write32_fixedbits(MSR_IA32_VMX_PROCBASED_CTLS,
			CPU_BASED_VM_EXEC_CONTROL,
			CPU_BASED_HLT_EXITING         /* 20.6.2 */
			| CPU_BASED_CR8_LOAD_EXITING    /* 20.6.2 */
			| CPU_BASED_CR8_STORE_EXITING   /* 20.6.2 */
			| CPU_BASED_UNCOND_IO_EXITING   /* 20.6.2 */
			| CPU_BASED_MOV_DR_EXITING
			| CPU_BASED_USE_TSC_OFFSETING   /* 21.3 */
			);

	//vmcs_write32(EXCEPTION_BITMAP, 1 << PF_VECTOR);
	vmcs_write32(PAGE_FAULT_ERROR_CODE_MASK, 0);
	vmcs_write32(PAGE_FAULT_ERROR_CODE_MATCH, 0);
	//vmcs_write32(CR3_TARGET_COUNT, 0);           /* 22.2.1 */

	vmcs_writel(HOST_CR0, read_cr0());  /* 22.2.3 */
	vmcs_writel(HOST_CR4, read_cr4());  /* 22.2.3, 22.2.5 */
	vmcs_writel(HOST_CR3, read_cr3());  /* 22.2.3  FIXME: shadow tables */

	vmcs_write16(HOST_CS_SELECTOR, __KERNEL_CS);  /* 22.2.4 */
	vmcs_write16(HOST_DS_SELECTOR, __KERNEL_DS);  /* 22.2.4 */
	vmcs_write16(HOST_ES_SELECTOR, __KERNEL_DS);  /* 22.2.4 */
	vmcs_write16(HOST_FS_SELECTOR, read_fs());    /* 22.2.4 */
	vmcs_write16(HOST_GS_SELECTOR, read_gs());    /* 22.2.4 */
	vmcs_write16(HOST_SS_SELECTOR, __KERNEL_DS);  /* 22.2.4 */
	
	//rdmsrl(MSR_FS_BASE, a);
	//vmcs_writel(HOST_FS_BASE, a); /* 22.2.4 */
	//rdmsrl(MSR_GS_BASE, a);
	//vmcs_writel(HOST_GS_BASE, a); /* 22.2.4 */

	vmcs_write16(HOST_TR_SELECTOR, GDT_ENTRY_TSS*8);  /* 22.2.4 */

#if 0
	get_idt(&dt);
	vmcs_writel(HOST_IDTR_BASE, dt.base);   /* 22.2.4 */
#endif

	vmcs_writel(HOST_RIP, (unsigned long)shiv_vmx_return); /* 22.2.5 */

	//rdmsr(MSR_IA32_SYSENTER_CS, host_sysenter_cs, junk);
	//vmcs_write32(HOST_IA32_SYSENTER_CS, host_sysenter_cs);
	//rdmsrl(MSR_IA32_SYSENTER_ESP, a);
	//vmcs_writel(HOST_IA32_SYSENTER_ESP, a);   /* 22.2.3 */
	//rdmsrl(MSR_IA32_SYSENTER_EIP, a);
	//vmcs_writel(HOST_IA32_SYSENTER_EIP, a);   /* 22.2.3 */
/*
	for (i = 0; i < NR_VMX_MSR; ++i) {
		u32 index = vmx_msr_index[i];
		u32 data_low, data_high;
		u64 data;
		int j = vcpu->nmsrs;

		if (rdmsr_safe(index, &data_low, &data_high) < 0)
			continue;
		if (wrmsr_safe(index, data_low, data_high) < 0)
			continue;
		data = data_low | ((u64)data_high << 32);
		vcpu->host_msrs[j].index = index;
		vcpu->host_msrs[j].reserved = 0;
		vcpu->host_msrs[j].data = data;
		vcpu->guest_msrs[j] = vcpu->host_msrs[j];
		++vcpu->nmsrs;
	}
	printk(KERN_DEBUG "kvm: msrs: %d\n", vcpu->nmsrs);
*/

	//nr_good_msrs = vcpu->nmsrs - NR_BAD_MSRS;
#if 0
	vmcs_writel(VM_ENTRY_MSR_LOAD_ADDR,
			virt_to_phys(vcpu->guest_msrs + NR_BAD_MSRS));
	vmcs_writel(VM_EXIT_MSR_STORE_ADDR,
			virt_to_phys(vcpu->guest_msrs + NR_BAD_MSRS));
	vmcs_writel(VM_EXIT_MSR_LOAD_ADDR,
			virt_to_phys(vcpu->host_msrs + NR_BAD_MSRS));
#endif

	vmcs_write32_fixedbits(MSR_IA32_VMX_EXIT_CTLS, VM_EXIT_CONTROLS,
			(1 << 9));  /* 22.2,1, 20.7.1 */ /* ??? */
	
#if 0
	vmcs_write32(VM_EXIT_MSR_STORE_COUNT, nr_good_msrs); /* 22.2.2 */
	vmcs_write32(VM_EXIT_MSR_LOAD_COUNT, nr_good_msrs);  /* 22.2.2 */
	vmcs_write32(VM_ENTRY_MSR_LOAD_COUNT, nr_good_msrs); /* 22.2.2 */
#endif

	/* 22.2.1, 20.8.1 */
	vmcs_write32_fixedbits(MSR_IA32_VMX_ENTRY_CTLS,
			VM_ENTRY_CONTROLS, 0);
	vmcs_write32(VM_ENTRY_INTR_INFO_FIELD, 0);  /* 22.2.1 */

	vmcs_writel(VIRTUAL_APIC_PAGE_ADDR, 0);
	vmcs_writel(TPR_THRESHOLD, 0);

	vmcs_writel(CR0_GUEST_HOST_MASK, SHIV_GUEST_CR0_MASK);
	vmcs_writel(CR4_GUEST_HOST_MASK, SHIV_GUEST_CR4_MASK);

	vcpu->cr0 = 0x60000010;
	vmcs_writel(CR0_READ_SHADOW, vcpu->cr0);
	vmcs_writel(GUEST_CR0,
			(vcpu->cr0 & ~SHIV_GUEST_CR0_MASK) | SHIV_VM_CR0_ALWAYS_ON);
	vmcs_writel(CR4_READ_SHADOW, 0);
	vmcs_writel(GUEST_CR4, SHIV_RMODE_VM_CR4_ALWAYS_ON);
	vcpu->cr4 = 0;

	return 0;

out:
	return ret;
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

