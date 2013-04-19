#ifndef TABLES_H
#define TABLES_H
#include <types.h>
#include <tss.h>
struct gdt_entry_struct
{
	u16int limit_low;
	u16int base_low;
	u8int  base_middle; 
	u8int  access;
	u8int  granularity;
	u8int  base_high; 
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct
{
	u16int limit;
	u32int base;
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

struct idt_entry_struct
{
	u16int base_lo;
	u16int sel;
	u8int  always0;
	u8int  flags;
	u16int base_hi; 
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

struct idt_ptr_struct
{
	u16int limit;
	u32int base;
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

extern void isr0 ();
extern void isr1 ();
extern void isr2 ();
extern void isr3 ();
extern void isr4 ();
extern void isr5 ();
extern void isr6 ();
extern void isr7 ();
extern void isr8 ();
extern void isr9 ();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr80();

extern void irq0 ();
extern void irq1 ();
extern void irq2 ();
extern void irq3 ();
extern void irq4 ();
extern void irq5 ();
extern void irq6 ();
extern void irq7 ();
extern void irq8 ();
extern void irq9 ();
extern void irq10 ();
extern void irq11 ();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

extern void ipi_panic();
extern void ipi_debug();
extern void ipi_shutdown();
extern void ipi_sched();
extern void ipi_tlb_ack();
extern void ipi_tlb();

void init_descriptor_tables(void);
void set_kernel_stack(tss_entry_t *, u32int stack);
void load_doublefault_system(void);
void write_tss(gdt_entry_t *, tss_entry_t *, s32int num, u16int ss0, u32int esp0);
void gdt_set_gate(gdt_entry_t *, s32int,u32int,u32int,u8int,u8int);
void idt_set_gate(u8int,u32int,u16int,u8int);
void mask_pic_int(unsigned char irq, int mask);
void disable_pic();

#endif
