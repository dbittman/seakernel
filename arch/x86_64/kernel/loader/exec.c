#include <task.h>
void arch_specific_exec_initializer(task_t *t, unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	t->sysregs->useresp = t->sysregs->rbp = STACK_LOCATION - STACK_ELEMENT_SIZE;
	t->sysregs->rdi = argc;
	t->sysregs->rsi = (uint64_t)t->argv;
	t->sysregs->rdx = (uint64_t)t->env;
	
	t->sysregs->eip = eip;
}
