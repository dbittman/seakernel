#ifndef TSS_x86_64_H
#define TSS_x86_64_H
#include <types.h>

typedef struct tss_entry_struct
{
	u32int prev_tss;
	u64int esp0;       // The stack pointer to load when we change to kernel mode.
	u32int ss0;        // The stack segment to load when we change to kernel mode.
	u64int esp1;       // Unused...
	u32int ss1;
	u64int esp2;
	u32int ss2;
	u64int cr3;
	u64int rip;
	u64int rflags;
	u64int rax;
	u64int rcx;
	u64int rdx;
	u64int rbx;
	u64int rsp;
	u64int rbp;
	u64int rsi;
	u64int rdi;
	u32int es;         // The value to load into ES when we change to kernel mode.
	u32int cs;         // The value to load into CS when we change to kernel mode.
	u32int ss;         // The value to load into SS when we change to kernel mode.
	u32int ds;         // The value to load into DS when we change to kernel mode.
	u32int fs;         // The value to load into FS when we change to kernel mode.
	u32int gs;         // The value to load into GS when we change to kernel mode.
	u32int ldt;        // Unused...
	u16int trap;
	u16int iomap_base;
} __attribute__((packed)) tss_entry_t;

extern void tss_flush();

#endif
