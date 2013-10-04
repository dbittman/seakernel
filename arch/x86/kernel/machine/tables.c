#include <kernel.h>
#include <tables-x86.h>
#include <isr.h>
#include <tss-x86.h>
#include <cpu.h>
static void init_idt();
gdt_entry_t gdt_entries[NUM_GDT_ENTRIES];
gdt_ptr_t   gdt_ptr;
idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;
void int_sys_init();
tss_entry_t tss_entry;

inline void set_kernel_stack(tss_entry_t *tss, u32int stack)
{
	tss->esp0 = stack;
}

void write_tss(gdt_entry_t *gdt, tss_entry_t *tss, s32int num, u16int ss0, u32int esp0)
{
	u32int base = (u32int)tss;
	u32int limit = sizeof(tss_entry_t);
	gdt_set_gate(gdt, num, base, limit, 0xE9, 0);
	memset(tss, 0, sizeof(tss_entry_t));
	tss->ss0  = ss0;  // Set the kernel stack segment.
	tss->esp0 = esp0; // Set the kernel stack pointer.
	tss->cs   = 0x0b;
	tss->ss = tss->ds = tss->es = tss->fs = tss->gs = 0x13;
} 

void init_gdt(gdt_entry_t *gdt, gdt_ptr_t *ptr)
{
	ptr->limit = (sizeof(gdt_entry_t) * NUM_GDT_ENTRIES) - 1;
	ptr->base  = (u32int)gdt;
	gdt_set_gate(gdt, 0, 0, 0, 0, 0);                // Null segment
	gdt_set_gate(gdt, 1, 0, 0xFFFFF, 0x98, 0xC); // Code segment
	gdt_set_gate(gdt, 2, 0, 0xFFFFF, 0x92, 0xC); // Data segment
	gdt_set_gate(gdt, 3, 0, 0xFFFFF, 0xF8, 0xC); // User mode code segment
	gdt_set_gate(gdt, 4, 0, 0xFFFFF, 0xF2, 0xC); // User mode data segment
	gdt_flush((u32int)ptr);
}

void gdt_set_gate(gdt_entry_t *gdt, s32int num, u32int base, u32int limit, u8int access, u8int gran)
{
	gdt[num].base_low    = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high   = (base >> 24) & 0xFF;
	gdt[num].limit_low   = (limit & 0xFFFF);
	gdt[num].granularity = (limit >> 16) & 0x0F;
	gdt[num].granularity |= (gran & 0x0F) << 4;
	gdt[num].access      = access;
}

static 
void io_wait( void )
{
    asm ( "jmp 1f\n\t"
                  "1:jmp 2f\n\t"
                  "2:" );
}
#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
void disable_pic()
{
	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);
}

void mask_pic_int(unsigned char irq, int mask)
{
	uint16_t port;
    uint8_t value;
    if(irq >= 8) {
		port = PIC2_DATA;
		irq -= 8;
	} else
		port = PIC1_DATA;
	if(mask)
		value = inb(port) | (1 << irq);
	else
		value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void init_pic()
{
	
	outb(0x20, 0x11);
	io_wait();
	outb(0xA0, 0x11);
	io_wait();
	outb(0x21, 0x20);
	io_wait();
	outb(0xA1, 0x28);
	io_wait();
	outb(0x21, 0x04);
	io_wait();
	outb(0xA1, 0x02);
	io_wait();
	outb(0x21, 0x01);
	io_wait();
	outb(0xA1, 0x01);
	io_wait();
	outb(0x21, 0x0);
	io_wait();
	outb(0xA1, 0x0);
	io_wait();
	interrupt_controller = IOINT_PIC;
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
#if CONFIG_SMP
	idt_set_gate(IPI_PANIC, (u32int)ipi_panic, 0x08, 0x8E);
	idt_set_gate(IPI_TLB, (u32int)ipi_tlb, 0x08, 0x8E);
	idt_set_gate(IPI_TLB_ACK, (u32int)ipi_tlb_ack, 0x08, 0x8E);
	idt_set_gate(IPI_SHUTDOWN, (u32int)ipi_shutdown, 0x08, 0x8E);
	idt_set_gate(IPI_DEBUG, (u32int)ipi_debug, 0x08, 0x8E);
	idt_set_gate(IPI_SCHED, (u32int)ipi_sched, 0x08, 0x8E);
#endif

	idt_flush((u32int)&idt_ptr);
}

void idt_set_gate(u8int num, u32int base, u16int sel, u8int flags)
{
	idt_entries[num].base_lo = base & 0xFFFF;
	idt_entries[num].base_hi = (base >> 16) & 0xFFFF;
	idt_entries[num].sel     = sel;
	idt_entries[num].always0 = 0;
	idt_entries[num].flags   = flags | 0x60;
}

/* each CPU gets it's own GDT and TSS, so we need to specify that here. */
void load_tables_ap(cpu_t *cpu)
{
	init_gdt(cpu->gdt, &cpu->gdt_ptr);
	/* don't init the IDT again, just flush it into the current processor.
	 * if init_idt is called, this can cause random GPF */
	idt_flush((u32int)&idt_ptr);
	write_tss(cpu->gdt, &cpu->tss, 5, 0x10, 0x0);
	tss_flush();
}

void load_tables()
{
	/* load up some temporary tables so we can use interrupts until the CPU stuff
	 * is loaded */
	init_gdt(gdt_entries, &gdt_ptr);
	init_idt();
	init_pic();
	int_sys_init();
}

void exceptionHandler(int a, void *p)
{
	idt_set_gate(a, (u32int)p , 0x08, 0x8E);
	idt_flush((u32int)&idt_ptr);
}