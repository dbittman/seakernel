#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/mm/vmm.h>
void arch_loader_exec_initializer(unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	/* TODO: shit like this is messy, clean it up */
	struct thread *t = current_thread;
	assert(t->sysregs);
	t->sysregs->useresp = t->sysregs->ebp = STACK_LOCATION - STACK_ELEMENT_SIZE;
	*(unsigned *)t->sysregs->useresp = (unsigned)t->process->env;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	*(unsigned *)t->sysregs->useresp = (unsigned)t->process->argv;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	*(unsigned *)t->sysregs->useresp = (unsigned)argc;
	t->sysregs->useresp -= STACK_ELEMENT_SIZE;
	
	t->sysregs->eip = eip;
}
