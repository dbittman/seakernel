#ifndef __ARCH_X86_SEA_TM_THREAD_X86_H
#define __ARCH_X86_SEA_TM_THREAD_X86_H

struct arch_thread_data {
	char fpu_save_data[512 + 16 /* alignment */];
};

struct thread_switch_context {
	uint32_t edi, esi, ebx, ebp, eflags;
};

#define tm_thread_context_basepointer(tsc) (tsc->ebp)

#endif

