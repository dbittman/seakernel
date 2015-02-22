#ifndef __MOD_SHIV_H
#define __MOD_SHIV_H
#include <sea/config.h>
#if CONFIG_MODULE_SHIV

#if CONFIG_ARCH != TYPE_ARCH_X86_64
#error "shiv only supports x86_64"
#endif

#include <sea/types.h>
#include <sea/cpu/processor.h>
/* many of these definitions are taken from the Linux SHIV project */

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
	int rtu_cause;
};


#define SHIV_RTU_IRQ_WINDOW_OPEN  1
#define SHIV_RTU_IO_INSTRUCTION   2
struct vcpu {
	struct vmcs *vmcs;
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
struct vcpu *shiv_create_vcpu(struct vmachine *vm);
int shiv_init_virtual_machine(struct vmachine *vm);
int vmx_vcpu_run(struct vcpu *vcpu);
int shiv_userspace_inject_interrupt(struct vcpu *vc, int irq);
int shiv_userspace_request_interruptible(struct vcpu *vc);

#endif
#endif

