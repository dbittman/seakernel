#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/mm/vmm.h>
void arch_loader_exec_initializer(unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	struct thread *t = current_thread;
	assert(t->regs);
	t->regs->useresp = t->regs->ebp = t->usermode_stack_end;
	*(unsigned *)t->regs->useresp = (unsigned)t->process->env;
	t->regs->useresp -= STACK_ELEMENT_SIZE;
	*(unsigned *)t->regs->useresp = (unsigned)t->process->argv;
	t->regs->useresp -= STACK_ELEMENT_SIZE;
	*(unsigned *)t->regs->useresp = (unsigned)argc;
	t->regs->useresp -= STACK_ELEMENT_SIZE;
	
	t->regs->eip = eip;
}
