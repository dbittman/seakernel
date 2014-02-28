#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <init.h>

#define SIGSTACK (STACK_LOCATION - (STACK_SIZE + PAGE_SIZE + 8))

int arch_userspace_signal_initializer(task_t *t, struct sigaction *sa)
{
	volatile registers_t *iret = t->regs;
	if(!iret) return 0;
	/* user-space signal handing design:
		* 
		* we exploit the fact the iret pops back everything in the stack, and that
		* we have access to those stack element. We trick iret into popping
		* back modified values of things, after pushing what looks like the
		* first half of a subprocedure call to the new stack location for the 
		* signal handler.
		* 
		* We set the return address of this 'function call' to be a bit of 
		* injector code, listed above, which simply does a system call (128).
		* This syscall copies back the original interrupt stack frame and 
		* immediately goes back to the isr common handler to perform the iret
		* back to where we were executing before.
		*/
	memcpy((void *)&t->reg_b, (void *)iret, sizeof(registers_t));
	iret->useresp = SIGSTACK;
	iret->useresp -= STACK_ELEMENT_SIZE;
	/* push the argument (signal number) */
	*(unsigned *)(iret->useresp) = t->sigd;
	iret->useresp -= STACK_ELEMENT_SIZE;
	/* push the return address. this function is mapped in when
		* paging is set up */
	*(unsigned *)(iret->useresp) = (unsigned)SIGNAL_INJECT;
	iret->eip = (unsigned)sa->_sa_func._sa_handler;
	t->cursig = t->sigd;
	t->sigd=0;
	/* sysregs is only set when we are in a syscall */
	if(t->sysregs) tm_process_raise_flag(t, TF_JUMPIN);
	return 1;
}
