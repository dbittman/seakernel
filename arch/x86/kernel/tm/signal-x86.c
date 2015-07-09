#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/boot/init.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/thread.h>
#define SIGSTACK (STACK_LOCATION - (STACK_SIZE + PAGE_SIZE + 8))

void arch_tm_userspace_signal_initializer(registers_t *regs, struct sigaction *sa)
{
	/* user-space signal handing design:
		* 
		* we exploit the fact the iret pops back everything in the stack, and that
		* we have access to those stack element. We trick regs into popping
		* back modified values of things, after pushing what looks like the
		* first half of a subprocedure call to the new stack location for the 
		* signal handler.
		* 
		* We set the return address of this 'function call' to be a bit of 
		* injector code, listed above, which simply does a system call (128).
		* This syscall copies back the original interrupt stack frame and 
		* immediately goes back to the isr common handler to perform the regs
		* back to where we were executing before.
		*/
	memcpy((void *)((addr_t)regs->useresp - sizeof(registers_t)), (void *)regs, sizeof(*regs));
	regs->useresp -= sizeof(registers_t);
	regs->useresp -= STACK_ELEMENT_SIZE;
	/* push the argument (signal number) */
	*(unsigned *)(regs->useresp) = current_thread->signal;
	regs->useresp -= STACK_ELEMENT_SIZE;
	/* push the return address. this function is mapped in when
		* paging is set up */
	*(unsigned *)(regs->useresp) = (unsigned)SIGNAL_INJECT;
	regs->eip = (unsigned)sa->_sa_func._sa_handler;
	current_thread->signal=0;
}

void arch_tm_userspace_signal_cleanup(registers_t *regs)
{
	memcpy((void *)regs, (void *)(regs->useresp + STACK_ELEMENT_SIZE), sizeof(*regs));
}

