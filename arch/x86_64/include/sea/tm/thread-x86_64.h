#ifndef __ARCH_X86_64_SEA_TM_THREAD_X86_64_H
#define __ARCH_X86_64_SEA_TM_THREAD_X86_64_H

struct arch_thread_data {
	char fpu_save_data[512 + 16 /* alignment */];
};

struct thread_switch_context {
	uint64_t r15, r14, r13, r12, rbx, rbp, rflags;
};

#define tm_thread_context_basepointer(tsc) ((tsc)->rbp)

#endif

