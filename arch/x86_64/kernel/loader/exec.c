#include <task.h>
void arch_specific_exec_initializer(task_t *t, unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	t->sysregs->useresp = t->sysregs->rbp = STACK_LOCATION - STACK_ELEMENT_SIZE;
	*(addr_t *)t->sysregs->useresp = (addr_t)t->env;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	*(addr_t *)t->sysregs->useresp = (addr_t)t->argv;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	*(addr_t *)t->sysregs->useresp = (addr_t)argc;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	
	t->sysregs->eip = eip;
}
