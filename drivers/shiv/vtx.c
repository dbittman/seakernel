#include <sea/loader/module.h>
#include <sea/types.h>
#include <sea/errno.h>
#include <sea/cpu/processor.h>
#include <sea/tm/process.h>
#include <sea/cpu/x86msr.h>
#include <sea/asm/system.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>
#include <sea/vsprintf.h>
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/tables-x86_64.h>
#include "vtx.h"
#include <modules/shiv.h>
#include <sea/lib/bitmap.h>
#include <sea/fs/devfs.h>
#include <sea/dm/char.h>
extern idt_ptr_t idt_ptr;
extern gdt_ptr_t gdt_ptr;

uint32_t revision_id; /* this is actually only 31 bits, but... */
addr_t vmxon_region=0;

#define __pa(x) (addr_t)mm_vm_get_map((addr_t)x,0,0)
extern __attribute__((regparm(0))) void vmx_return(void);
#define __KERNEL_CS 0x8
#define __KERNEL_DS 0x10
#define GDT_ENTRY_TSS 5
#define offsetof(a,b) __builtin_offsetof(a,b)
static void vmcs_clear(struct vmcs *vmcs)
{
	uint64_t phys_addr = __pa(vmcs);
	uint8_t error;
	printk(0, "vmcs clear!\n");
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

void vmcs_writel(unsigned long field, unsigned long value)
{
	uint8_t error;

	asm (ASM_VMX_VMWRITE_RAX_RDX "; setna %0"
			: "=q"(error) : "a"(value), "d"(field) : "cc" );
	if (unlikely(error))
		vmwrite_error(field, value);
}

void vmcs_write16(unsigned long field, uint16_t value)
{
	vmcs_writel(field, value);
}

void vmcs_write32(unsigned long field, uint32_t value)
{
	vmcs_writel(field, value);
}

void vmcs_write64(unsigned long field, uint64_t value)
{
	vmcs_writel(field, value);
}

void vmcs_write32_fixedbits(uint32_t msr, uint32_t vmcs_field, uint32_t val)
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
	printk(0, "shiv: vmx_vcpu_load\n");

	if(vcpu->cpu != cpu)
		vcpu_clear(vcpu);

	uint8_t error;

	asm (ASM_VMX_VMPTRLD_RAX "; setna %0"
			: "=g"(error) : "a"(&phys_addr), "m"(phys_addr)
			: "cc");
	if (error)
		printk(2, "kvm: vmptrld %x/%x fail\n",
				vcpu->vmcs, phys_addr);
	else
		vcpu->loaded = 1;

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

	/* write the revision ID to the vmxon region */
	uint32_t *vmxon_id_ptr = (uint32_t *)(vmxon_region + PHYS_PAGE_MAP);
	*vmxon_id_ptr = revision_id;

	/* magic code */
	asm(".byte 0xf3, 0x0f, 0xc7, 0x30"::"a"(&vmxon_region), "m"(vmxon_region):"memory", "cc");
	return 1;
}

int shiv_vmx_off()
{

}

addr_t ept_mm_alloc_physical_page_zero()
{
	addr_t ret = mm_alloc_physical_page();
	memset((void *)(ret + PHYS_PAGE_MAP), 0, 0x1000);
	return ret;
}

int ept_vm_map(pml4_t *pml4, addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	addr_t vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);

	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	addr_t x = ept_mm_alloc_physical_page_zero();
	if(!pml4[vp4])
		pml4[vp4] = x | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = ept_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		pd[vdir] = ept_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);

	pt[vtbl] = (phys & PAGE_MASK) | attr;

	if(!(opt & MAP_NOCLEAR)) 
		memset((void *)(virt&PAGE_MASK), 0, 0x1000);

	return 0;
}

/* returns physical address */
addr_t shiv_build_ept_pml4(struct vcpu *vc, addr_t memsz)
{
	addr_t pml4 = mm_alloc_physical_page();
	uint8_t *vpml4 = (void *)(pml4 + PHYS_PAGE_MAP);
	memset(vpml4, 0, 0x1000);
	/* hack: limit memory */
	if(memsz > 256)
		memsz = 256;
	if(memsz < 256)
		memsz = 256;
	printk(0, "[shiv]: building an EPT of size %d pages\n", memsz, memsz / 1024);
	vc->pages = kmalloc(sizeof(addr_t) * memsz);
	for(addr_t i=0;i<memsz;i++) {
		vc->pages[i] = mm_alloc_physical_page();
		ept_vm_map((void *)(pml4 + PHYS_PAGE_MAP), i * 0x1000, vc->pages[i], 7, MAP_NOCLEAR); 
	}
	uint8_t *vinit = (void *)(vc->pages[255] + PHYS_PAGE_MAP);
	memset(vinit, 0, 0x1000);
	vinit[0xFF0] = 0xb8;
	vinit[0xFF1] = 0x34;
	vinit[0xFF2] = 0x12;
	vinit[0xFF3] = 0xea;
	vinit[0xff4] = 0;
	vinit[0xff5] = 0xf0;
	vinit[0xff6] = 0;
	vinit[0xff7] = 0xf0;

	printk(0, "[shiv]: writing interrupt code\n");
	uint32_t *ivt = (void *)(vc->pages[0] + PHYS_PAGE_MAP);
	ivt[3] = 0xF000ff00;
	printk(0, "[shiv]: writing interrupt handler code\n");
	vinit[0xf00] = 0xb8;
	vinit[0xf01] = 0x78;
	vinit[0xf02] = 0x56;
	vinit[0xf03] = 0xcf; /* hlt */
	printk(0, "[shiv]: okay\n");

	printk(4, "[shiv]: writing fake bios to location %x\n", 255 * 0x1000);
	memcpy(vinit, bios, sizeof(bios));
	return pml4;
}

void shiv_skip_instruction(struct vcpu *vc)
{
	/* so, ignoring an instruction actually takes work. */
	uint32_t len = vmcs_read32(VM_EXIT_INSTRUCTION_LEN);
	unsigned long rip = vmcs_readl(GUEST_RIP);
	rip += len;
	vmcs_writel(GUEST_RIP, rip);
	/* something something interrupts TODO */
}

void shiv_inject_interrupt(struct vcpu *vc, int irq)
{
	assert(bitmap_test(vc->irq_field, irq));
	bitmap_reset(vc->irq_field, irq);
	if(vc->mode == CPU_X86_MODE_RMODE) {
		/* rmode interrupt injection requires a bit more work */
		/* TODO: Do they? */
	} else {
		vmcs_write32(VM_ENTRY_INTR_INFO_FIELD, irq | INTR_TYPE_EXT_INTR | INTR_INFO_VALID_MASK);
	}
}

int shiv_vcpu_pending_int(struct vcpu *vc)
{
	return bitmap_ffs(vc->irq_field, 256);
}

void shiv_handle_irqs(struct vcpu *vc)
{
	vc->interruptible = ((vmcs_readl(GUEST_RFLAGS) & (1 << 9)/* IF */) && ((vmcs_read32(GUEST_INTERRUPTIBILITY_INFO) & 3) == 0));
	if (vc->interruptible && (shiv_vcpu_pending_int(vc) != -1)
			&& !(vmcs_read32(VM_ENTRY_INTR_INFO_FIELD) & INTR_INFO_VALID_MASK)) {
		shiv_inject_interrupt(vc, shiv_vcpu_pending_int(vc));
	}
	uint32_t tmp = vmcs_read32(CPU_BASED_VM_EXEC_CONTROL);
	if (!vc->interruptible 
			&& (shiv_vcpu_pending_int(vc) != -1 || vc->request_interruptible)) {
		tmp |= CPU_BASED_VIRTUAL_INTR_PENDING;
		kprintf("setting pending int exit (%d, %x, %d)\n", vc->interruptible, shiv_vcpu_pending_int(vc), vc->request_interruptible);
	} else {
		tmp &= ~CPU_BASED_VIRTUAL_INTR_PENDING;
	}
	vmcs_write32(CPU_BASED_VM_EXEC_CONTROL, tmp);
}

int exit_reason_interrupt(struct vcpu *vc)
{
	if((shiv_vcpu_pending_int(vc) == -1) && vc->request_interruptible) {
		vc->run.rtu_cause = SHIV_RTU_IRQ_WINDOW_OPEN;
		return 0;
	}
	return 1;
}

int exit_reason_halt(struct vcpu *vc)
{
	uint64_t rip = vmcs_readl(GUEST_RIP);
	kprintf("HALTED at %x!\n", rip);
	shiv_skip_instruction(vc);
	if(shiv_vcpu_pending_int(vc))
		return 1;
	return 0;
}

int exit_reason_io(struct vcpu *vc)
{
	uint64_t eq = vmcs_read64(EXIT_QUALIFICATION);
	vc->run.rtu_cause = SHIV_RTU_IO_INSTRUCTION;

	vc->run.io.in = (eq & 8);
	vc->run.io.rep = (eq & 32) != 0;
	vc->run.io.size = (eq & 7) + 1;
	vc->run.io.string = (eq & 16) != 0;
	vc->run.io.stringdown = (vmcs_readl(GUEST_RFLAGS) & (1 << 10)) != 0;
	vc->run.io.port = eq >> 16;
	if(vc->run.io.string)
		vc->run.io.addr = vmcs_readl(GUEST_LINEAR_ADDRESS);
	else
		vc->run.io.value = vc->regs[VCPU_REGS_RAX];

	printk(5, "--> IO instruction [%x]: %d %d %d %x %x\n",
			vmcs_readl(GUEST_RIP),
			vc->run.io.in,
			vc->run.io.rep,
			vc->run.io.size,
			vc->run.io.port,
			vc->run.io.value);

	shiv_skip_instruction(vc);
	return 0;
}

int exit_reason_cr_access(struct vcpu *vc)
{
	unsigned long exit_qualification, val;
	int cr;
	int reg;
	int err;

	exit_qualification = vmcs_readl(EXIT_QUALIFICATION);
	cr = exit_qualification & 15;
	reg = (exit_qualification >> 8) & 15;
	printk(4, "CR access! (cr=%d, reg=%d)\n", cr, reg);
	switch ((exit_qualification >> 4) & 3) {
		case 0: /* mov to cr */
			val = vc->regs[reg];
			switch (cr) {
				case 0:
					printk(4, "writing CR0 with %x\n", val);
					vmcs_writel(GUEST_CR0, val);
					shiv_skip_instruction(vc);
					return 1;
				case 3:
					printk(4, "writing CR3 with %x\n", val);
					vmcs_writel(GUEST_CR3, val);
					shiv_skip_instruction(vc);
					return 1;
			}
			break;
	}
	return 1;
}

int exit_reason_external_interrupt(struct vcpu *vc)
{
	return -EINTR;
}

int exit_reason_nmi(struct vcpu *vc)
{
	uint32_t eb,errc;
	eb = vmcs_read32(VM_EXIT_INTR_INFO);
	errc = vmcs_read32(VM_EXIT_INTR_ERROR_CODE);
	uint64_t rip = vmcs_readl(GUEST_RIP);

	addr_t cr2 = vc->cr2;
	
	printk(5, "Exit due to NMI (PF) (eb = %x, err = %x, grip = %x, cr2 = %x)", eb, errc, rip, cr2);
	panic(0, "");
	return 0;
}

void *exitreasons [NUM_EXIT_REASONS] = {
	[EXIT_REASON_HLT] = exit_reason_halt,
	[EXIT_REASON_CR_ACCESS] = exit_reason_cr_access,
	[EXIT_REASON_PENDING_INTERRUPT] = exit_reason_interrupt,
	[EXIT_REASON_EXTERNAL_INTERRUPT] = exit_reason_external_interrupt,
	[EXIT_REASON_IO_INSTRUCTION] = exit_reason_io,
	[EXIT_REASON_EXCEPTION_NMI] = exit_reason_nmi
};

int shiv_vm_exit_handler(struct vcpu *vcpu)
{
	//printk(0, "got to exit handler\n");
	/* check cause of exit */
	void *fn = exitreasons[vcpu->exit_reason];
	if(!fn)
		panic(0, "[shiv]: exit reason not defined (%d)\n", vcpu->exit_reason);
	/* handle exit reasons */
	int (*exithandler)(void *) = fn;
	return exithandler(vcpu);
	/* return to VM */
}

struct vmcs *shiv_alloc_vmcs()
{
	return kmalloc_a(0x1000);
}

void shiv_init_vmcs(struct vcpu *vc)
{
	printk(0, "shiv: init_vmcs\n");
	/* allocate a vmcs for a cpu by calling shiv_alloc_vmcs */
	vc->vmcs = shiv_alloc_vmcs();
	/* initialize the region as needed */
	vc->vmcs->rev_id = revision_id & 0x7FFFFFFF;
	vmcs_clear(vc->vmcs);
}

static void seg_setup(int seg)
{
	struct vmx_segment_field *sf = &vmx_segment_fields[seg];

	vmcs_write16(sf->selector, 0);
	vmcs_writel(sf->base, 0);
	vmcs_write32(sf->limit, 0xffff);
	vmcs_write32(sf->ar_bytes, 0x8093);
}

/*
 * Sets up the vmcs for emulated real mode.
 */
int shiv_vcpu_setup(struct vcpu *vcpu)
{
	/* TODO: preemption timer */
	int i;
	int ret = 0;
	int use_ept = vcpu->flags & SHIV_VCPU_FLAG_USE_EPT;
	printk(0, "shiv: vcpu_setup\n");

	memset(vcpu->regs, 0, sizeof(vcpu->regs));
	vcpu->regs[VCPU_REGS_RDX] = 0x600;
	vcpu->apic_base = 0xfee00000 |
		/*for vcpu 0*/ MSR_IA32_APICBASE_BSP |
		MSR_IA32_APICBASE_ENABLE;

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

	vmcs_write64(VMCS_LINK_POINTER, -1ull); /* 22.3.1.5 */

	/* Special registers */
	vmcs_write64(GUEST_IA32_DEBUGCTL, 0);

	/* Control */
	/* we exit on external interrupts, but we do NOT ack them during a VM
	 * exit. This allows us to handle external interrupts normally
	 * as soon as we restore host state and re-enable interrupts */
	vmcs_write32_fixedbits(MSR_IA32_VMX_PINBASED_CTLS,
			PIN_BASED_VM_EXEC_CONTROL,
			PIN_BASED_EXT_INTR_MASK   /* 20.6.1 */
			| PIN_BASED_NMI_EXITING   /* 20.6.1 */
			/*		| PIN_BASED_PREEMPT_TIMER */
			);
	vmcs_write32_fixedbits(MSR_IA32_VMX_PROCBASED_CTLS,
			CPU_BASED_VM_EXEC_CONTROL,
			CPU_BASED_HLT_EXITING         /* 20.6.2 */
			| CPU_BASED_CR8_LOAD_EXITING    /* 20.6.2 */
			| CPU_BASED_CR8_STORE_EXITING   /* 20.6.2 */
			| CPU_BASED_UNCOND_IO_EXITING   /* 20.6.2 */
			| CPU_BASED_MOV_DR_EXITING
			| CPU_BASED_USE_TSC_OFFSETING   /* 21.3 */
			| CPU_BASED_ACTIVATE_SECONDARY
			);

	uint32_t secondary_flags = SECONDARY_EXEC_CTL_UNRESTRICTED;
	if(use_ept) {
		secondary_flags |= SECONDARY_EXEC_CTL_ENABLE_EPT;
	}
	vmcs_write32(SECONDARY_VM_EXEC_CONTROL, secondary_flags);

	/* we let all exceptions be handled by the guest, since we're
	 * doing EPT and unrestricted. */
	vmcs_write32(EXCEPTION_BITMAP, 0);
	vmcs_write32(PAGE_FAULT_ERROR_CODE_MASK, 0);
	vmcs_write32(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	vmcs_writel(HOST_CR0, read_cr0());  /* 22.2.3 */
	vmcs_writel(HOST_CR4, read_cr4());  /* 22.2.3, 22.2.5 */
	vmcs_writel(HOST_CR3, read_cr3());  /* 22.2.3  FIXME: shadow tables */

	vmcs_write16(HOST_CS_SELECTOR, __KERNEL_CS);  /* 22.2.4 */
	vmcs_write16(HOST_DS_SELECTOR, __KERNEL_DS);  /* 22.2.4 */
	vmcs_write16(HOST_ES_SELECTOR, __KERNEL_DS);  /* 22.2.4 */
	vmcs_write16(HOST_FS_SELECTOR, read_fs());    /* 22.2.4 */
	vmcs_write16(HOST_GS_SELECTOR, read_gs());    /* 22.2.4 */
	vmcs_write16(HOST_SS_SELECTOR, __KERNEL_DS);  /* 22.2.4 */

	vmcs_write16(HOST_TR_SELECTOR, (GDT_ENTRY_TSS * 8));  /* 22.2.4 */
	printk(0, "[shiv]: wrote %x for tr select\n", (GDT_ENTRY_TSS * 8));

	vmcs_writel(HOST_IDTR_BASE, vcpu->cpu->arch_cpu_data.idt_ptr.base);   /* 22.2.4 */
	vmcs_writel(HOST_GDTR_BASE, vcpu->cpu->arch_cpu_data.gdt_ptr.base);   /* 22.2.4 */
	vmcs_writel(HOST_TR_BASE, (unsigned long)(&vcpu->cpu->arch_cpu_data.tss));
	printk(0, "[shiv]: wrote %x for idt bases\n", vcpu->cpu->arch_cpu_data.idt_ptr.base);
	printk(0, "[shiv]: wrote %x for gdt base\n", vcpu->cpu->arch_cpu_data.gdt_ptr.base);
	printk(0, "[shiv]: wrote %x for tr base\n", &vcpu->cpu->arch_cpu_data.tss);

	vmcs_writel(HOST_RIP, (unsigned long)vmx_return); /* 22.2.5 */

	uint32_t se_cs = read_msr(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFF;
	vmcs_write32(HOST_IA32_SYSENTER_CS, se_cs);
	uint32_t tmp = read_msr(MSR_IA32_SYSENTER_ESP);
	vmcs_writel(HOST_IA32_SYSENTER_ESP, tmp);   /* 22.2.3 */
	tmp = read_msr(MSR_IA32_SYSENTER_EIP);
	vmcs_writel(HOST_IA32_SYSENTER_EIP, tmp);   /* 22.2.3 */

	vmcs_write32_fixedbits(MSR_IA32_VMX_EXIT_CTLS, VM_EXIT_CONTROLS,
			(1 << 9));  /* 22.2,1, 20.7.1 */ /* ??? */

	/* 22.2.1, 20.8.1 */
	vmcs_write32_fixedbits(MSR_IA32_VMX_ENTRY_CTLS,
			VM_ENTRY_CONTROLS, 0);
	vmcs_write32(VM_ENTRY_INTR_INFO_FIELD, 0);  /* 22.2.1 */

	vmcs_writel(VIRTUAL_APIC_PAGE_ADDR, 0);
	vmcs_writel(TPR_THRESHOLD, 0);

	//vmcs_writel(CR0_GUEST_HOST_MASK, SHIV_GUEST_CR0_MASK);
	vmcs_writel(CR4_GUEST_HOST_MASK, SHIV_GUEST_CR4_MASK);

	vcpu->cr0 = 0x60000010;
	vmcs_writel(CR0_READ_SHADOW, vcpu->cr0 | CR0_PE_MASK | CR0_PG_MASK);
	vmcs_writel(GUEST_CR0,
			(vcpu->cr0 & ~SHIV_GUEST_CR0_MASK) | SHIV_VM_CR0_ALWAYS_ON);
	vmcs_writel(CR4_READ_SHADOW, 0);
	vmcs_writel(GUEST_CR4, SHIV_RMODE_VM_CR4_ALWAYS_ON);
	vcpu->cr4 = 0;

	if(use_ept) {
		/* set up the EPT */
		addr_t ept = shiv_build_ept_pml4(vcpu, 0x10000);
		ept |= (3 << 3) | 6;
		vmcs_write64(EPT_POINTER, ept);
	}
	printk(0, "[shiv]: CPU setup!\n");

	return 0;

out:
	return ret;
}

struct vcpu *shiv_create_vcpu(struct vmachine *vm, int flags)
{
	printk(0, "shiv: create vcpu\n");
	/* create structure */
	vm->vcpu = kmalloc(sizeof(*vm->vcpu));
	vm->vcpu->cpu = current_task->cpu;
	vm->vcpu->flags = flags;
	/* set up guest CPU state as needed */
	shiv_init_vmcs(vm->vcpu);
	vmx_vcpu_load(current_task->cpu, vm->vcpu);
	if(flags & SHIV_VCPU_FLAG_RESTRICTED) {
		shiv_vcpu_setup_longmode(vm->vcpu);
	} else {
		shiv_vcpu_setup(vm->vcpu);
	}
	/* return vcpu */
	return vm->vcpu;
}

int shiv_init_virtual_machine(struct vmachine *vm)
{
	/* set id */
	return 0;
}

#define FX_IMAGE_GUEST 0
#define FX_IMAGE_HOST  1
#define MSR_IMAGE_GUEST 0
#define MSR_IMAGE_HOST  1

void fx_save(struct vcpu *vcpu, int selector)
{
	if(vcpu->cpu->flags & CPU_SSE || vcpu->cpu->flags & CPU_FPU)
		__asm__ __volatile__("fxsave64 (%0)"
				:: "r" (ALIGN(vcpu->fpu_save_data[selector], 16)));
}

void fx_restore(struct vcpu *vcpu, int selector)
{
	if(vcpu->cpu->flags & CPU_SSE || vcpu->cpu->flags & CPU_FPU)
		__asm__ __volatile__("fxrstor64 (%0)"
				:: "r" (ALIGN(vcpu->fpu_save_data[selector], 16)));
}

void tss_reload(cpu_t *cpu)
{
	/* the vm exit process only loads the selector into the TR register, but doesn't
	 * actually read the data out of the GDT. So, we have to flush the TSS after each
	 * exit. */
	cpu->arch_cpu_data.gdt[GDT_ENTRY_TSS].access = 0xE9;
	asm("mov $0x2B, %%ax\n"
			"ltr %%ax":::"ax");
}

void save_msrs(struct vcpu *vcpu, int selector)
{
	int i;
	for(i=0;i<NR_MSRS;i++) {
		vcpu->msrs[selector][i] = read_msr(MSRS[i]);
	}
}

void restore_msrs(struct vcpu *vcpu, int selector)
{
	/* guest msrs are invalid, since they haven't been saved before.
	 * Only restore after first launch */
	if(!vcpu->launched && selector == MSR_IMAGE_GUEST)
		return;
	int i;
	for(i=0;i<NR_MSRS;i++) {
		write_msr(MSRS[i], vcpu->msrs[selector][i]);
	}
}

int vmx_vcpu_run(struct vcpu *vcpu)
{
	uint8_t fail;
	uint16_t fs_sel, gs_sel, ldt_sel;
	int fs_gs_ldt_reload_needed;
	int r;

	assert(vcpu->loaded);
again:
	/*
	 * Set host fs and gs selectors.  Unfortunately, 22.2.3 does not
	 * allow segment selectors with cpl > 0 or ti == 1.
	 */
	fs_sel = read_fs();
	gs_sel = read_gs();
	vmcs_write16(HOST_FS_SELECTOR, fs_sel);
	vmcs_write16(HOST_GS_SELECTOR, gs_sel);

	vmcs_writel(HOST_FS_BASE, read_msr(0xc0000100 /* 64bit FS base */));
	vmcs_writel(HOST_GS_BASE, read_msr(0xc0000101 /* 64bit GS base */));
	vcpu->run.rtu_cause = 0; /* don't automatically return to userspace */

	shiv_handle_irqs(vcpu);

	/* the host state must be saved. Registers, flags, and FPU states aren't
	 * saved, and neither are many MSRs. We back up the MSRs we care about, save
	 * the FPU state, and push pretty much everything onto the stack. After
	 * a vmexit, everything gets popped off, the FPU state restored, and the
	 * MSRs put back. That way, it's all as we left it. */

	fx_save(vcpu, FX_IMAGE_HOST);
	fx_restore(vcpu, FX_IMAGE_GUEST);
	save_msrs(vcpu, MSR_IMAGE_HOST);
	restore_msrs(vcpu, MSR_IMAGE_GUEST);

	//printk(4, "[shiv]: launching VM\n");
	int intflag = cpu_interrupt_set(0);
	asm (
			/* Store host registers */
			"cli \n\t"
			"pushf \n\t"
			"push %%rax; push %%rbx; push %%rdx;"
			"push %%rsi; push %%rdi; push %%rbp;"
			"push %%r8;  push %%r9;  push %%r10; push %%r11;"
			"push %%r12; push %%r13; push %%r14; push %%r15;"
			"push %%rcx \n\t"
			ASM_VMX_VMWRITE_RSP_RDX "\n\t"
			/* Check if vmlaunch of vmresume is needed */
			"cmp $0, %1 \n\t"
			/* Load guest registers.  Don't clobber flags. */
			"mov %c[cr2](%3), %%rax \n\t"
			"mov %%rax, %%cr2 \n\t"
			"mov %c[rax](%3), %%rax \n\t"
			"mov %c[rbx](%3), %%rbx \n\t"
			"mov %c[rdx](%3), %%rdx \n\t"
			"mov %c[rsi](%3), %%rsi \n\t"
			"mov %c[rdi](%3), %%rdi \n\t"
			"mov %c[rbp](%3), %%rbp \n\t"
			"mov %c[r8](%3),  %%r8  \n\t"
			"mov %c[r9](%3),  %%r9  \n\t"
			"mov %c[r10](%3), %%r10 \n\t"
			"mov %c[r11](%3), %%r11 \n\t"
			"mov %c[r12](%3), %%r12 \n\t"
			"mov %c[r13](%3), %%r13 \n\t"
			"mov %c[r14](%3), %%r14 \n\t"
			"mov %c[r15](%3), %%r15 \n\t"
			"mov %c[rcx](%3), %%rcx \n\t" /* kills %3 (rcx) */
			"xchg %%bx, %%bx\n\t"
			/* Enter guest mode */
			"jne launched \n\t"
			ASM_VMX_VMLAUNCH "\n\t"
			"jmp vmx_return \n\t"
			"launched: " ASM_VMX_VMRESUME "\n\t"
			".globl vmx_return \n\t"
			"vmx_return: "
			/* Save guest registers, load host registers, keep flags */
			"xchg %3,     (%%rsp) \n\t"
			"mov %%rax, %c[rax](%3) \n\t"
			"mov %%rbx, %c[rbx](%3) \n\t"
			"pushq (%%rsp); popq %c[rcx](%3) \n\t"
			"mov %%rdx, %c[rdx](%3) \n\t"
			"mov %%rsi, %c[rsi](%3) \n\t"
			"mov %%rdi, %c[rdi](%3) \n\t"
			"mov %%rbp, %c[rbp](%3) \n\t"
			"mov %%r8,  %c[r8](%3) \n\t"
			"mov %%r9,  %c[r9](%3) \n\t"
			"mov %%r10, %c[r10](%3) \n\t"
			"mov %%r11, %c[r11](%3) \n\t"
			"mov %%r12, %c[r12](%3) \n\t"
			"mov %%r13, %c[r13](%3) \n\t"
			"mov %%r14, %c[r14](%3) \n\t"
			"mov %%r15, %c[r15](%3) \n\t"
			"mov %%cr2, %%rax   \n\t"
			"mov %%rax, %c[cr2](%3) \n\t"
			"mov (%%rsp), %3 \n\t"

			"pop  %%rcx; pop  %%r15; pop  %%r14; pop  %%r13; pop  %%r12;"
			"pop  %%r11; pop  %%r10; pop  %%r9;  pop  %%r8;"
			"pop  %%rbp; pop  %%rdi; pop  %%rsi;"
			"pop  %%rdx; pop  %%rbx; pop  %%rax \n\t"
			"setbe %0 \n\t"
			"popf \n\t" /* interrupts are still disabled */
			: "=q" (fail)
			: "r"(vcpu->launched), "d"((unsigned long)HOST_RSP),
			"c"(vcpu),
			[rax]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RAX])),
			[rbx]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RBX])),
			[rcx]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RCX])),
			[rdx]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RDX])),
			[rsi]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RSI])),
			[rdi]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RDI])),
			[rbp]"i"(offsetof(struct vcpu, regs[VCPU_REGS_RBP])),
			[r8 ]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R8 ])),
			[r9 ]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R9 ])),
			[r10]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R10])),
			[r11]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R11])),
			[r12]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R12])),
			[r13]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R13])),
			[r14]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R14])),
			[r15]"i"(offsetof(struct vcpu, regs[VCPU_REGS_R15])),
			[cr2]"i"(offsetof(struct vcpu, cr2))
				  : "cc", "memory" );

	fx_save(vcpu, FX_IMAGE_GUEST);
	fx_restore(vcpu, FX_IMAGE_HOST);

	save_msrs(vcpu, MSR_IMAGE_GUEST);
	restore_msrs(vcpu, MSR_IMAGE_HOST);

	vcpu->interruptible = (vmcs_read32(GUEST_INTERRUPTIBILITY_INFO) & 3) == 0;

	vcpu->exit_type = 0;
	if (fail) {
		vcpu->exit_type = SHIV_EXIT_TYPE_FAIL_ENTRY;
		vcpu->exit_reason = vmcs_read32(VM_INSTRUCTION_ERROR);
		r = 0;
	} else {
		int old = cpu_interrupt_set(0);
		/* restore fs and gs */
		asm ("mov %0, %%gs" : : "rm"(gs_sel));
		asm ("mov %0, %%fs" : : "rm"(fs_sel));
		write_msr(0xc0000101 /* 64bit GS base */, vmcs_readl(HOST_GS_BASE));
		tss_reload(vcpu->cpu);
		cpu_interrupt_set(old);

		vcpu->launched = 1;
		vcpu->exit_type = SHIV_EXIT_TYPE_VM_EXIT;
		vcpu->exit_reason = vmcs_readl(VM_EXIT_REASON);
		r = shiv_vm_exit_handler(vcpu);
		if (r > 0) {
			if(tm_process_got_signal(current_task)) {
				return -EINTR;
			}
			goto again;
		}
	}
	cpu_interrupt_set(intflag);
	return r;
}

