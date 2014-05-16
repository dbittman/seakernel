#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
void arch_loader_exec_initializer(task_t *t, unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	assert(t->sysregs);
	t->sysregs->useresp = t->sysregs->ebp = STACK_LOCATION - STACK_ELEMENT_SIZE;
	*(unsigned *)t->sysregs->useresp = (unsigned)t->env;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	*(unsigned *)t->sysregs->useresp = (unsigned)t->argv;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	*(unsigned *)t->sysregs->useresp = (unsigned)argc;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	
	t->sysregs->eip = eip;
}
