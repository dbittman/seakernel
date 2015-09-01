#ifndef __SEA_TM_PTRACE_H
#define __SEA_TM_PTRACE_H

#include <sea/cpu/ptrace_user.h>

enum __ptrace_request {
	PTRACE_TRACEME     = 0,
	PTRACE_DETACH      = 1,
	PTRACE_PEEKTEXT    = 2,
	PTRACE_PEEKDATA    = 2,
	//PTRACE_PEEKUSER    = 3,
	PTRACE_POKETEXT    = 4,
	PTRACE_POKEDATA    = 4,
	//PTRACE_POKEUSER    = 5,
	//PTRACE_GETREGS     = 6,
	//PTRACE_SETREGS     = 7,
	PTRACE_GETSIGINFO  = 8,
	PTRACE_SETSIGINFO  = 9,
	PTRACE_SETOPTIONS  = 10,
	PTRACE_GETEVENTMSG = 11,
	PTRACE_CONT        = 12,
	PTRACE_SYSCALL     = 13,
	PTRACE_SINGLESTEP  = 14,
	PTRACE_LISTEN      = 15,
	PTRACE_INTERRUPT   = 16,
	PTRACE_ATTACH      = 17,
	PTRACE_SEIZE       = 18,
	PTRACE_READUSER    = 19,
	PTRACE_WRITEUSER   = 20,
};

enum __ptrace_option {
	PTRACE_O_EXITKILL       = (1 << 0),
	PTRACE_O_TRACECLONE     = (1 << 1),
	PTRACE_O_TRACEEXEC      = (1 << 2),
	PTRACE_O_TRACEEXIT      = (1 << 3),
	PTRACE_O_TRACEFORK      = (1 << 4),
	PTRACE_O_TRACESYSGOOD   = (1 << 5),
	PTRACE_O_TRACEVFORK     = (1 << 6),
	PTRACE_O_TRACEVFORKDONE = (1 << 7),
};

enum __ptrace_event {
	PTRACE_EVENT_VFORK      = 1,
	PTRACE_EVENT_VFORK_DONE = 2,
	PTRACE_EVENT_FORK       = 3,
	PTRACE_EVENT_EXEC       = 4,
	PTRACE_EVENT_EXIT       = 5,
	PTRACE_EVENT_STOP       = 6,
};

long sys_ptrace(enum __ptrace_request request, pid_t tid, void *addr, void *data);

int arch_cpu_ptrace_read_user(struct thread *thread, struct ptrace_user *user);
int arch_cpu_ptrace_write_user(struct thread *thread, struct ptrace_user *user);
#define TRACEE_STOPON_SYSCALL 1

#endif

