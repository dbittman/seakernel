#include <sea/kernel.h>
#include <sea/loader/module.h>
#include <sea/types.h>

#include <modules/shiv.h>

int shiv_check_hardware_support()
{
	/* checks for existance of VMX, and reads in features information */
}

int shiv_vmx_on()
{

}

int shiv_vmx_off()
{

}

addr_t shiv_alloc_vmx_data_area()
{

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
	kprintf("SHIV - starting up\n");
	return 0;
}

int module_exit()
{
	return 0;
}

