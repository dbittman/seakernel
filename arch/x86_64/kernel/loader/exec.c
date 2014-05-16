#include <sea/tm/process.h>
void arch_loader_exec_initializer(task_t *t, unsigned argc, addr_t eip)
{
	/* don't ya just love iret? */
	assert(t->sysregs);
	t->sysregs->rdi = argc;
	t->sysregs->rsi = (uint64_t)t->argv;
	t->sysregs->rdx = (uint64_t)t->env;
	
		/* don't ya just love iret? */
	if(t->flags & TF_OTHERBS) {
		kprintf("not implemented - x86_64 32-bit ELF\n");
		t->sysregs->useresp = t->sysregs->rbp = 0xFFFF0000 - 4;
		*(unsigned *)t->sysregs->useresp = (unsigned)(addr_t)t->env;
		t->sysregs->useresp -= 4;
		*(unsigned *)t->sysregs->useresp = (unsigned)(addr_t)t->argv;
		t->sysregs->useresp -= 4;
		*(unsigned *)t->sysregs->useresp = (unsigned)argc;
		t->sysregs->useresp -= 4;
	} else
		t->sysregs->useresp = t->sysregs->rbp = STACK_LOCATION - 8;
	
	t->sysregs->eip = eip;
}
