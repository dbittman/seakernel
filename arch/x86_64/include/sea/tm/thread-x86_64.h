#ifndef __ARCH_X86_64_SEA_TM_THREAD_X86_64_H
#define __ARCH_X86_64_SEA_TM_THREAD_X86_64_H

struct arch_thread_data {
	char fpu_save_data[512 + 16 /* alignment */];
};

#endif

