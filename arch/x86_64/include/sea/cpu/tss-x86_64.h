#ifndef TSS_x86_64_H
#define TSS_x86_64_H
#include <sea/types.h>

typedef struct tss_entry_struct
{
	uint32_t prev_tss;
	uint64_t esp0;       // The stack pointer to load when we change to kernel mode.
	uint32_t ss0;        // The stack segment to load when we change to kernel mode.
	uint64_t esp1;       // Unused...
	uint32_t ss1;
	uint64_t esp2;
	uint32_t ss2;
	uint64_t cr3;
	uint64_t rip;
	uint64_t rflags;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint32_t es;         // The value to load into ES when we change to kernel mode.
	uint32_t cs;         // The value to load into CS when we change to kernel mode.
	uint32_t ss;         // The value to load into SS when we change to kernel mode.
	uint32_t ds;         // The value to load into DS when we change to kernel mode.
	uint32_t fs;         // The value to load into FS when we change to kernel mode.
	uint32_t gs;         // The value to load into GS when we change to kernel mode.
	uint32_t ldt;        // Unused...
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

extern void tss_flush();

#endif
