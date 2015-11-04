#include <sea/cpu/ptrace_user.h>
#include <sea/cpu/registers.h>
#include <sea/errno.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
int arch_cpu_ptrace_read_user(struct thread *thread, struct ptrace_user *user)
{
	if(!thread->regs) {
		return -ENOTSUP;
	}
	struct registers regs;
	mm_context_read(&thread->process->vmm_context, (void *)&regs, (addr_t)thread->regs, sizeof(regs));
	user->regs.rbx = regs.rbx;
	user->regs.rdx = regs.rdx;
	user->regs.rsi = regs.rsi;
	user->regs.rdi = regs.rdi;
	user->regs.rcx = regs.rcx;
	user->regs.rbp = regs.rbp;
	user->regs.rsp = regs.useresp;
	user->regs.rip = regs.eip;
	user->regs.orig_rax = thread->orig_syscall ? thread->orig_syscall : regs.rax;
	user->regs.rax = thread->orig_syscall ? (unsigned long)thread->syscall_return : regs.rax;
	user->regs.ds = regs.ds;
	user->regs.fs = regs.ds;
	user->regs.gs = regs.ds;
	user->regs.es = regs.ds;
	user->regs.cs = regs.cs;
	user->regs.ss = regs.ss;
	user->regs.eflags = regs.eflags;
	return 0;
}

int arch_cpu_ptrace_write_user(struct thread *thread, struct ptrace_user *user)
{
	return -ENOTSUP;
}

