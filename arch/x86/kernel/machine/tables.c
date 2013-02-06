#include <kernel.h>
#include <tables.h>
#include <isr.h>
#include <tss.h>
extern void gdt_flush(u32int);
extern void idt_flush(u32int);
void init_gdt();
static void init_idt();
gdt_entry_t gdt_entries[6];
gdt_ptr_t   gdt_ptr;
idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;
extern isr_t interrupt_handlers[];
void int_sys_init();
tss_entry_t tss_entry;
extern char tables;

inline void set_kernel_stack(u32int stack)
{
	tss_entry.esp0 = stack;
}

void write_tss(s32int num, u16int ss0, u32int esp0)
{
	u32int base = (u32int) &tss_entry;
	u32int limit = base + sizeof(tss_entry);
	gdt_set_gate(num, base, limit, 0xE9, 0x00);
	memset(&tss_entry, 0, sizeof(tss_entry));
	tss_entry.ss0  = ss0;  // Set the kernel stack segment.
	tss_entry.esp0 = esp0; // Set the kernel stack pointer.
	tss_entry.cs   = 0x0b;
	tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;
} 

void init_gdt()
{
	gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
	gdt_ptr.base  = (u32int)&gdt_entries;
	gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
	gdt_set_gate(1, 0, 0xFFFFF, 0x98, 0xC); // Code segment
	gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xC); // Data segment
	gdt_set_gate(3, 0, 0xFFFFF, 0xF8, 0xC); // User mode code segment
	gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xC); // User mode data segment
	gdt_flush((u32int)&gdt_ptr);
}

void gdt_set_gate(s32int num, u32int base, u32int limit, u8int access, u8int gran)
{
	gdt_entries[num].base_low    = (base & 0xFFFF);
	gdt_entries[num].base_middle = (base >> 16) & 0xFF;
	gdt_entries[num].base_high   = (base >> 24) & 0xFF;
	gdt_entries[num].limit_low   = (limit & 0xFFFF);
	gdt_entries[num].granularity = (limit >> 16) & 0x0F;
	gdt_entries[num].granularity |= (gran & 0x0F) << 4;
	gdt_entries[num].access      = access;
}

void init_pic()
{
	interrupt_controller = IOINT_PIC;
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);
}

static void init_idt()
{
	idt_ptr.limit = sizeof(idt_entry_t) * 256 -1;
	idt_ptr.base  = (u32int)&idt_entries;
	memset(&idt_entries, 0, sizeof(idt_entry_t)*256);
	idt_set_gate( 0, (u32int)isr0 , 0x08, 0x8E);
	idt_set_gate( 1, (u32int)isr1 , 0x08, 0x8E);
	idt_set_gate( 2, (u32int)isr2 , 0x08, 0x8E);
	idt_set_gate( 3, (u32int)isr3 , 0x08, 0x8E);
	idt_set_gate( 4, (u32int)isr4 , 0x08, 0x8E);
	idt_set_gate( 5, (u32int)isr5 , 0x08, 0x8E);
	idt_set_gate( 6, (u32int)isr6 , 0x08, 0x8E);
	idt_set_gate( 7, (u32int)isr7 , 0x08, 0x8E);
	idt_set_gate( 8, (u32int)isr8 , 0x08, 0x8E);
	idt_set_gate( 9, (u32int)isr9 , 0x08, 0x8E);
	idt_set_gate(10, (u32int)isr10, 0x08, 0x8E);
	idt_set_gate(11, (u32int)isr11, 0x08, 0x8E);
	idt_set_gate(12, (u32int)isr12, 0x08, 0x8E);
	idt_set_gate(13, (u32int)isr13, 0x08, 0x8E);
	idt_set_gate(14, (u32int)isr14, 0x08, 0x8E);
	idt_set_gate(15, (u32int)isr15, 0x08, 0x8E);
	idt_set_gate(16, (u32int)isr16, 0x08, 0x8E);
	idt_set_gate(17, (u32int)isr17, 0x08, 0x8E);
	idt_set_gate(18, (u32int)isr18, 0x08, 0x8E);
	idt_set_gate(19, (u32int)isr19, 0x08, 0x8E);
	idt_set_gate(20, (u32int)isr20, 0x08, 0x8E);
	idt_set_gate(21, (u32int)isr21, 0x08, 0x8E);
	idt_set_gate(22, (u32int)isr22, 0x08, 0x8E);
	idt_set_gate(23, (u32int)isr23, 0x08, 0x8E);
	idt_set_gate(24, (u32int)isr24, 0x08, 0x8E);
	idt_set_gate(25, (u32int)isr25, 0x08, 0x8E);
	idt_set_gate(26, (u32int)isr26, 0x08, 0x8E);
	idt_set_gate(27, (u32int)isr27, 0x08, 0x8E);
	idt_set_gate(28, (u32int)isr28, 0x08, 0x8E);
	idt_set_gate(29, (u32int)isr29, 0x08, 0x8E);
	idt_set_gate(30, (u32int)isr30, 0x08, 0x8E);
	idt_set_gate(31, (u32int)isr31, 0x08, 0x8E);
	idt_set_gate(32, (u32int)irq0, 0x08, 0x8E);
	idt_set_gate(33, (u32int)irq1, 0x08, 0x8E);
	idt_set_gate(34, (u32int)irq2, 0x08, 0x8E);
	idt_set_gate(35, (u32int)irq3, 0x08, 0x8E);
	idt_set_gate(36, (u32int)irq4, 0x08, 0x8E);
	idt_set_gate(37, (u32int)irq5, 0x08, 0x8E);
	idt_set_gate(38, (u32int)irq6, 0x08, 0x8E);
	idt_set_gate(39, (u32int)irq7, 0x08, 0x8E);
	idt_set_gate(40, (u32int)irq8, 0x08, 0x8E);
	idt_set_gate(41, (u32int)irq9, 0x08, 0x8E);
	idt_set_gate(42, (u32int)irq10, 0x08, 0x8E);
	idt_set_gate(43, (u32int)irq11, 0x08, 0x8E);
	idt_set_gate(44, (u32int)irq12, 0x08, 0x8E);
	idt_set_gate(45, (u32int)irq13, 0x08, 0x8E);
	idt_set_gate(46, (u32int)irq14, 0x08, 0x8E);
	idt_set_gate(47, (u32int)irq15, 0x08, 0x8E);
	/* 0x80 is syscall */
	idt_set_gate(0x80, (u32int)isr80, 0x08, 0x8E);
	
	idt_flush((u32int)&idt_ptr);
}

void idt_set_gate(u8int num, u32int base, u16int sel, u8int flags)
{
	idt_entries[num].base_lo = base & 0xFFFF;
	idt_entries[num].base_hi = (base >> 16) & 0xFFFF;
	idt_entries[num].sel     = sel;
	idt_entries[num].always0 = 0;
	idt_entries[num].flags   = flags  | 0x60;
	
}

void load_tables_ap()
{
	init_gdt();
	init_idt();
	write_tss(5, 0x10, 0x0);
	tss_flush();
}

void load_tables()
{
	init_gdt();
	init_idt();
	init_pic();
	memset(&interrupt_handlers, 0, sizeof(isr_t)*256);
	write_tss(5, 0x10, 0x0);
	tss_flush();
	int_sys_init();
	tables=1;
}
