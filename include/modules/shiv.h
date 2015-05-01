#ifndef __MOD_SHIV_H
#define __MOD_SHIV_H
#include <sea/config.h>
#if CONFIG_MODULE_SHIV

#if CONFIG_ARCH != TYPE_ARCH_X86_64
#error "shiv only supports x86_64"
#endif

#include <sea/types.h>
#include <sea/cpu/processor.h>

#define SHIV_EXIT_TYPE_FAIL_ENTRY 1
#define SHIV_EXIT_TYPE_VM_EXIT    2

struct sl_io {
	int rep;
	int port;
	int in;
	int string, stringdown;
	int size;
	long value, addr;
};

struct slaunch {
	struct sl_io io;
	int rtu_cause, error;
};

#define SHIV_RTU_ENTRY_ERROR      0
#define SHIV_RTU_IRQ_WINDOW_OPEN  1
#define SHIV_RTU_IO_INSTRUCTION   2

#define SHIV_VCPU_FLAG_USE_EPT    1
#define SHIV_VCPU_FLAG_RESTRICTED 2

struct vcpu {
	struct vmcs *vmcs;
	int flags;
	cpu_t *cpu;
	int launched, loaded;
	unsigned long cr0, cr2, cr4;
	int mode;
	addr_t *pages;
	unsigned long regs[NR_VCPU_REGS];
	unsigned long apic_base;
	uint32_t exit_type, exit_reason;
	char fpu_save_data[2][512 + 16 /* alignment */];
	uint64_t msrs[2][NR_MSRS];
	int interruptible, request_interruptible;
	uint64_t irq_field[4];
	struct slaunch run;
};

struct vmachine {
	int id;
	struct vcpu *vcpu;
};

#define VM_CREATE  1
#define VM_DESTROY 2
#define VM_REQ_INT 3
#define VM_INJ_INT 4
#define VM_LAUNCH  5
#define VM_STOP    6
#define VM_STATUS  7
#define VM_MAP     8
#define VM_UNMAP   9
struct vmioctl {
	int flags;
	int irq;
	int min; /* for create */
	unsigned long us_vaddr, paddr;
	struct slaunch run;
};




int shiv_check_hardware_support();
int shiv_vmx_on();
int shiv_vcpu_setup(struct vcpu *vcpu);
struct vcpu *shiv_create_vcpu(struct vmachine *vm, int);
int shiv_init_virtual_machine(struct vmachine *vm);
int vmx_vcpu_run(struct vcpu *vcpu);
int shiv_userspace_inject_interrupt(struct vcpu *vc, int irq);
int shiv_userspace_request_interruptible(struct vcpu *vc);


#define ASM_VMX_VMCLEAR_RAX       ".byte 0x66, 0x0f, 0xc7, 0x30"
#define ASM_VMX_VMLAUNCH          ".byte 0x0f, 0x01, 0xc2"
#define ASM_VMX_VMRESUME          ".byte 0x0f, 0x01, 0xc3"
#define ASM_VMX_VMPTRLD_RAX       ".byte 0x0f, 0xc7, 0x30"
#define ASM_VMX_VMREAD_RDX_RAX    ".byte 0x0f, 0x78, 0xd0"
#define ASM_VMX_VMWRITE_RAX_RDX   ".byte 0x0f, 0x79, 0xd0"
#define ASM_VMX_VMWRITE_RSP_RDX   ".byte 0x0f, 0x79, 0xd4"
#define ASM_VMX_VMXOFF            ".byte 0x0f, 0x01, 0xc4"
#define ASM_VMX_VMXON_RAX         ".byte 0xf3, 0x0f, 0xc7, 0x30"

#define VMX_SEGMENT_FIELD(seg)                                  \
	[VCPU_SREG_##seg] = {                                   \
		.selector = GUEST_##seg##_SELECTOR,             \
		.base = GUEST_##seg##_BASE,                     \
		.limit = GUEST_##seg##_LIMIT,                   \
		.ar_bytes = GUEST_##seg##_AR_BYTES,             \
	}

static struct vmx_segment_field {
	unsigned selector;
	unsigned base;
	unsigned limit;
	unsigned ar_bytes;
} vmx_segment_fields[] = {
	VMX_SEGMENT_FIELD(CS),
	VMX_SEGMENT_FIELD(DS),
	VMX_SEGMENT_FIELD(ES),
	VMX_SEGMENT_FIELD(FS),
	VMX_SEGMENT_FIELD(GS),
	VMX_SEGMENT_FIELD(SS),
	VMX_SEGMENT_FIELD(TR),
	VMX_SEGMENT_FIELD(LDTR),
};

static inline unsigned long read_cr0(void)
{ 
	unsigned long cr0;
	asm ("movq %%cr0,%0" : "=r" (cr0));
	return cr0;
} 

static inline void write_cr0(unsigned long val) 
{ 
	asm ("movq %0,%%cr0" :: "r" (val));
} 

static inline unsigned long read_cr3(void)
{ 
	unsigned long cr3;
	asm("movq %%cr3,%0" : "=r" (cr3));
	return cr3;
} 

static inline unsigned long read_cr4(void)
{ 
	unsigned long cr4;
	asm("movq %%cr4,%0" : "=r" (cr4));
	return cr4;
} 

static inline void write_cr4(unsigned long val)
{ 
	asm ("movq %0,%%cr4" :: "r" (val));
} 
static inline u16 read_fs(void)
{
	u16 seg;
	asm ("mov %%fs, %0" : "=g"(seg));
	return seg;
}

static inline u16 read_gs(void)
{
	u16 seg;
	asm ("mov %%gs, %0" : "=g"(seg));
	return seg;
}

static unsigned char bios[] = {
  0xfa, 0xb8, 0x00, 0xf0, 0x8e, 0xd8, 0x0f, 0x01, 0x16, 0x38, 0xf0, 0x0f,
  0x20, 0xc0, 0x66, 0x83, 0xc8, 0x01, 0x0f, 0x22, 0xc0, 0x66, 0xea, 0x40,
  0xf0, 0x0f, 0x00, 0x08, 0x00, 0x90, 0x90, 0x90, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x9a, 0xcf, 0x00,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xcf, 0x00, 0x17, 0x00, 0x20, 0xf0,
  0x0f, 0x00, 0x90, 0x90, 0x66, 0xb8, 0x10, 0x00, 0x8e, 0xd8, 0x8e, 0xd0,
  0x8e, 0xc0, 0x8e, 0xe0, 0x8e, 0xe8, 0xbd, 0x00, 0x00, 0x09, 0x00, 0x89,
  0xec, 0xb8, 0x21, 0x43, 0x65, 0x87, 0xf4
};

#endif
#endif

