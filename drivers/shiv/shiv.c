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
#define MAX_VM 8

int next_vm_min=0;
struct vmachine *vmlist[MAX_VM];

int shiv_userspace_inject_interrupt(struct vcpu *vc, int irq)
{
	bitmap_set(vc->irq_field, irq);
	return 0;
}

int shiv_userspace_request_interruptible(struct vcpu *vc)
{
	vc->request_interruptible = 1;
	return 0;
}

struct vmachine *shiv_create_vmachine()
{
	struct vmachine *vm = kmalloc(sizeof(*vm));
	int ret = shiv_init_virtual_machine(vm);
	if(ret == -1) {
		kfree(vm);
		return 0;
	}
	vm->vcpu = shiv_create_vcpu(vm, SHIV_VCPU_FLAG_USE_EPT);
	return vm;
}

struct vmachine *shiv_get_vmachine(int min)
{
	return vmlist[min];
}

int shiv_maj;

int shiv_ioctl(int min, int cmd, long arg)
{
	//printk(0, "--> IOCTL %d %d\n", min, cmd);
	struct vmachine *vm = min == 0 ? 0 : shiv_get_vmachine(min);
	if(min && !vm)
		return -ENOENT;
	struct vmioctl *ctl = (void *)arg;
	int r;
	switch(cmd) {
		case VM_CREATE:
			if(vm)
				return -EINVAL;
			min = ++next_vm_min;
			if(min >= MAX_VM)
				return -EINVAL;
			vm = shiv_create_vmachine();
			vmlist[min] = vm;
			ctl->min = min;
			char tmp[7] = "shiv";
			tmp[4] = min + '0';
			tmp[5]=0;
			devfs_add(devfs_root, tmp, S_IFCHR, shiv_maj, min);
			break;
		case VM_DESTROY:

			break;
		case VM_LAUNCH:
			r = vmx_vcpu_run(vm->vcpu);
			if(r == -EINTR)
				return r;
			if(vm->vcpu->exit_type == SHIV_EXIT_TYPE_VM_EXIT) {
				assert(vm->vcpu->run.rtu_cause);
				memcpy(&ctl->run, &vm->vcpu->run, sizeof(ctl->run));
			} else {
				ctl->run.rtu_cause = SHIV_RTU_ENTRY_ERROR;
				ctl->run.error = vm->vcpu->exit_reason;
			}
			
			return r;
			break;
		case VM_REQ_INT:
			shiv_userspace_request_interruptible(vm->vcpu);
			break;
		case VM_INJ_INT:
			shiv_userspace_inject_interrupt(vm->vcpu, ctl->irq);
			break;
		case VM_STATUS:

			break;
		case VM_STOP:

			break;
		case VM_MAP:
			mm_vm_map(ctl->us_vaddr, vm->vcpu->pages[ctl->paddr / 0x1000], PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_NOCLEAR);
			break;
		case VM_UNMAP:
			mm_vm_unmap_only(ctl->us_vaddr, 0);
			break;
		default:
			kprintf("unknown SHIV ioctl (%d)\n", cmd);
			break;
	}
	return 0;
}

void exploit_setup_pagetables(addr_t *gcr3, addr_t *mcr3);

void vmcs_set_cr3_target(int n, uint64_t cr3);
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


	struct vmachine *vm = kmalloc(sizeof(*vm));
	shiv_init_virtual_machine(vm);
	struct vcpu *vcpu = shiv_create_vcpu(vm, SHIV_VCPU_FLAG_RESTRICTED);

	addr_t guest, monitor;
	exploit_setup_pagetables(&guest, &monitor);
	vmcs_set_cr3_target(0, guest);
	vmcs_set_cr3_target(0, monitor);

	vmcs_set_cr3(guest);
	

	vmx_vcpu_run(vcpu);
	

	
	shiv_maj = dm_set_available_char_device(0, shiv_ioctl, 0);
	devfs_add(devfs_root, "shivctl", S_IFCHR, shiv_maj, 0);
	return 0;
}

int module_exit()
{
	return 0;
}

