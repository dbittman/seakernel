#include <sea/cpu/ptrace_user.h>
#include <sea/cpu/registers.h>
#include <sea/errno.h>
#include <sea/tm/thread.h>
int arch_cpu_ptrace_read_user(struct thread *thread, struct ptrace_user *user)
{
	if(!thread->regs) {
		return -ENOTSUP;
	}
#if 0
	user->regs.ebx = thread->regs->ebx;
	user->regs.edx = thread->regs->edx;
	user->regs.esi = thread->regs->esi;
	user->regs.edi = thread->regs->edi;
	user->regs.ecx = thread->regs->ecx;
	user->regs.ebp = thread->regs->ebp;
	user->regs.esp = thread->regs->esp;
	user->regs.eip = thread->regs->eip;
	user->regs.orig_eax = thread->orig_syscall ? thread->orig_syscall : thread->regs->eax;
	user->regs.eax = thread->orig_syscall ? (unsigned long)thread->syscall_return : thread->regs->eax;
	user->regs.xds = thread->regs->ds;
	user->regs.xfs = thread->regs->ds;
	user->regs.xgs = thread->regs->ds;
	user->regs.xes = thread->regs->ds;
	user->regs.xcs = thread->regs->cs;
	user->regs.xss = thread->regs->ss;
	user->regs.eflags = thread->regs->eflags;
#endif
	return 0;
}

int arch_cpu_ptrace_write_user(struct thread *thread, struct ptrace_user *user)
{
	return -ENOTSUP;
}

