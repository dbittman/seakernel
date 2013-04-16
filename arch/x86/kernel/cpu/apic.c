/* Defines functions to program the LAPIC and the IOAPIC (if present) */
#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <cpu.h>
#include <task.h>
#include <mutex.h>
#include <imps.h>
#define MAX_IOAPIC 8
#define write_ioapic(l,o,v) ioapic_rw(l, WRITE, o, v)
#define read_ioapic(l,o) ioapic_rw(l, READ, o, 0)
unsigned lapic_timer_start=0;
volatile unsigned num_ioapic=0;
struct imps_ioapic *ioapic_list[MAX_IOAPIC];
extern char imcr_present;
extern int imps_enabled;
void add_ioapic(struct imps_ioapic *ioapic)
{
	assert(ioapic);
	assert(ioapic->type == 2);
	assert(num_ioapic < MAX_IOAPIC);
	ioapic_list[num_ioapic]=ioapic;
	ioapic_list[++num_ioapic]=0;
}

unsigned ioapic_rw(struct imps_ioapic *l, int rw, unsigned char off, unsigned val)
{
	*(uint32_t*)(l->addr) = off;
	if(rw == WRITE)
		return (*(uint32_t*)(l->addr + 0x10) = val);
	else
		return *(uint32_t*)(l->addr + 0x10);
}

void write_ioapic_vector(struct imps_ioapic *l, unsigned irq, char masked, char trigger, char polarity, char mode, unsigned char vector)
{
	unsigned lower=0, higher=0;
	lower = (unsigned)vector & 0xFF;
	/* 8-10: delivery mode */
	lower |= (mode << 8) & 0x700;
	/* 11: destination mode */
	//lower |= (1 << 11);
	/* 13: polarity */
	if(polarity) lower |= (1 << 13);
	/* 15: trigger */
	if(trigger) lower |= (1 << 15);
	/* 16: mask */
	if(masked) lower |= (1 << 16);
	/* 56-63: destination. Currently, we just send this to the bootstrap cpu */
	higher |= (bootstrap << 24) & 0xF;
	write_ioapic(l, irq*2 + 0x10, 0x10000);
	write_ioapic(l, irq*2 + 0x10 + 1, higher);
	write_ioapic(l, irq*2 + 0x10, lower);
}

int program_ioapic(struct imps_ioapic *ia)
{
	int i;
	for(i=0;i<16;i++) {
		if(i != 2)
			write_ioapic_vector(ia, i, 0, 0, 0, 0, 32+i);
	}
	return 1;
}

void lapic_eoi()
{
	if(!imps_enabled)
		return;
	IMPS_LAPIC_WRITE(LAPIC_EOI, 0x0);
}

void set_lapic_timer(unsigned tmp)
{
	if(!imps_enabled)
		return;
	kprintf("SET\n");
	IMPS_LAPIC_WRITE(LAPIC_LVTT, 32 | 0x20000);
	IMPS_LAPIC_WRITE(LAPIC_TDCR, 3);
	IMPS_LAPIC_WRITE(LAPIC_TICR, tmp);
}
#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
void calibrate_lapic_timer(unsigned freq)
{
	if(!imps_enabled)
		return;
	unsigned tmp=0;
	IMPS_LAPIC_WRITE(LAPIC_TDCR, 3);
	IMPS_LAPIC_WRITE(LAPIC_LVTT, 32);
	//cli();
	//outb(0x61, (inb(0x61) & 0xFD) | 1);
	//outb(0x43,0xB2);
	//1193180/100 Hz = 11931 = 2e9bh
	//outb(0x42,0x9B);	//LSB
	//inb(0x60);	//short delay
	//outb(0x42,0x2E);	//MSB
 
	//reset PIT one-shot counter (start counting)
	//tmp = inb(0x61)&0xFE;
	//outb(0x61,(uint8)tmp);		//gate low
	//outb(0x61,(uint8)tmp|1);		//gate high
	IMPS_LAPIC_WRITE(LAPIC_TICR, 0xFFFFFFFF);
	//delay_sleep(100);
	//while(!(inb(0x61)&0x20));
	unsigned current = IMPS_LAPIC_READ(LAPIC_TCCR);
	IMPS_LAPIC_WRITE(LAPIC_LVTT, 0x10000);
	tmp=(((0xFFFFFFFF-current)+1) * 100 * 16)/freq/16;
	if(tmp < 16) tmp = 16;
	tmp *= 512;
	lapic_timer_start = tmp;
	printk(5, "[apic]: set timer initial count to %d\n", tmp);
	cli();
	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);
	set_lapic_timer(0x100000);
	/*
	uint16_t port;
    uint8_t value;
    unsigned char irq = 0;
    port = PIC1_DATA;
    //irq -= 8;
    value = inb(port) | (1 << irq);
    outb(port, value); 
			outb(0xA1, 0xFF);
	outb(0x21, 0xFF); */
	sti();
	printk(7, "no more pic...\n");
	//IMPS_LAPIC_WRITE(LAPIC_EOI, 0);
	while(1) {
		sti();asm("sti");
		//current = IMPS_LAPIC_READ(LAPIC_TCCR);
		//unsigned q = IMPS_LAPIC_READ(LAPIC_LVTT);
		//unsigned r = IMPS_LAPIC_READ(LAPIC_ISR + 0x10 );
		//if(current) printk(0, "%x: %d: %x\n", q, current, r);
	}
	for(;;);
}

void init_lapic(int extint)
{
	if(!imps_enabled)
		return;
	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);
	unsigned long long lapic_msr = read_msr(0x1b);
	write_msr(0x1b, (lapic_msr&0xFFFFF000) | 0x800, 0); //set global enable bit for lapic
	kprintf(">> %x\n", lapic_msr);
	IMPS_LAPIC_WRITE(LAPIC_TPR, 0);
	int i;
	for(i=0;i<=255;i++)
		lapic_eoi();
	IMPS_LAPIC_WRITE(LAPIC_DFR, 0xFFFFFFFF);
	IMPS_LAPIC_WRITE(LAPIC_LDR, (IMPS_LAPIC_READ(LAPIC_LDR)&0x00FFFFFF)|1);
	IMPS_LAPIC_WRITE(LAPIC_LVTT, LAPIC_DISABLE);
	IMPS_LAPIC_WRITE(LAPIC_LVT0, 0x400 | (extint ? 0 : LAPIC_DISABLE)); //external interrupts
	IMPS_LAPIC_WRITE(LAPIC_LVT0, 0x8700 | (extint ? 0 : LAPIC_DISABLE)); //external interrupts
	IMPS_LAPIC_WRITE(LAPIC_LVTE, 0xFF | LAPIC_DISABLE); //NMI
	IMPS_LAPIC_WRITE(LAPIC_LVTPC, 0xFF | LAPIC_DISABLE); //NMI
	
	IMPS_LAPIC_WRITE(LAPIC_TPR, 0);
	IMPS_LAPIC_WRITE(LAPIC_SPIV, 0x0100 | 0xFF);
}

void id_map_apic(page_dir_t *pd)
{
	int a = PAGE_DIR_IDX(imps_lapic_addr / 0x1000);
	int t = PAGE_TABLE_IDX(imps_lapic_addr / 0x1000);
	pd[a] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	unsigned int *pt = (unsigned int *)(pd[a] & PAGE_MASK);
	pt[t] = (imps_lapic_addr&PAGE_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE;
}

void init_ioapic()
{return;
	if(!num_ioapic)
		return;
	unsigned i=0, num=0;
	/* Disable the PIC...*/
	cli();
	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);
	interrupt_controller = 0;
	for(;i<num_ioapic;i++) {
		struct imps_ioapic *l = ioapic_list[i];
		assert(l->type == 2);
		printk(1, "[apic]: found ioapic at %x: ID %d, version %x; flags=%x\n", l->addr, l->id, l->ver, l->flags);
		if(l->flags)
			num += program_ioapic(l);
	}
	if(!num)
	{
		/* We can still try to use the PIC... */
		kprintf("[apic]: WARNING: no IOAPIC controllers are enabled.\n[apic]: WARNING: this is a non-conforming system.\n");
		kprintf("[apic]: WARNING: using PIC only, advanced interrupt features disabled.\n");
		interrupt_controller = IOINT_PIC;
		init_pic();
	}
	else if(imcr_present) {
		outb(0x22, 0x70);
		outb(0x23, 0x01);
	}
	interrupt_controller = IOINT_APIC;
	sti();
}
#endif
