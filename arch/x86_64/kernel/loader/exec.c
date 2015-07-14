#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/vsprintf.h>
void arch_loader_exec_initializer(unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	struct thread *t = current_thread;
	assert(t->regs);
	t->regs->rdi = argc;
	t->regs->rsi = (uint64_t)t->process->argv;
	t->regs->rdx = (uint64_t)t->process->env;
	t->regs->useresp = t->regs->rbp = t->usermode_stack_end;
	
	t->regs->eip = eip;
}

