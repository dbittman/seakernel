/* Defines functions to program the LAPIC and the IOAPIC (if present) */
#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <cpu.h>
#include <task.h>
#include <sea/mutex.h>

#define MAX_IOAPIC 8
#define write_ioapic(l,o,v) ioapic_rw(l, WRITE, o, v)
#define read_ioapic(l,o) ioapic_rw(l, READ, o, 0)

struct ioapic {
	addr_t addr;
	int id, start;
};

unsigned lapic_timer_start=0;
volatile unsigned num_ioapic=0;
struct ioapic ioapic_list[MAX_IOAPIC];

void add_ioapic(addr_t address, int id, int int_start)
{
	assert(num_ioapic < MAX_IOAPIC);
	ioapic_list[num_ioapic].addr=address;
	ioapic_list[num_ioapic].id=id;
	ioapic_list[num_ioapic].start=int_start;
	ioapic_list[++num_ioapic].addr=0;
}

unsigned ioapic_rw(struct ioapic *l, int rw, unsigned char off, unsigned val)
{
	*(uint32_t*)((addr_t)l->addr) = off;
	if(rw == WRITE)
		return (*(uint32_t*)((addr_t)l->addr + 0x10) = val);
	else
		return *(uint32_t*)((addr_t)l->addr + 0x10);
}

void write_ioapic_vector(struct ioapic *l, unsigned irq, char masked, char trigger, char polarity, char mode, unsigned char vector)
{
	unsigned lower=0, higher=0;
	lower = (unsigned)vector & 0xFF;
	/* 8-10: delivery mode */
	lower |= (mode << 8) & 0x700;
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

int program_ioapic(struct ioapic *ia)
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
	if(!(kernel_state_flags & KSF_CPUS_RUNNING))
		return;
	LAPIC_WRITE(LAPIC_EOI, 0x0);
}

void set_lapic_timer(unsigned tmp)
{
	if(!(kernel_state_flags & KSF_CPUS_RUNNING))
		return;
	/* mask the old timer interrupt */
	mask_pic_int(0, 1);
	/* THE ORDER OF THESE IS IMPORTANT!
	 * If you write the initial count before you
	 * tell it to set periodic mode and set the vector
	 * it will not generate an interrupt! */
	LAPIC_WRITE(LAPIC_LVTT, 32 | 0x20000);
	LAPIC_WRITE(LAPIC_TDCR, 3);
	LAPIC_WRITE(LAPIC_TICR, tmp);
}

void calibrate_lapic_timer(unsigned freq)
{
	if(!(kernel_state_flags & KSF_CPUS_RUNNING))
		return;
	printk(0, "[smp]: calibrating LAPIC timer...");
	LAPIC_WRITE(LAPIC_LVTT, 32);
	LAPIC_WRITE(LAPIC_TDCR, 3);
	LAPIC_WRITE(LAPIC_TICR, 0xFFFFFFFF);
	
	/* wait 1/10 of a second */
	tm_delay_sleep(current_hz / 10);
	/* read how much it has counted and stop the timer */
	unsigned val = LAPIC_READ(LAPIC_TCCR);
	LAPIC_WRITE(LAPIC_LVTT, LAPIC_DISABLE);
	
	/* calculate how much to set the timer to */
	unsigned diff = (0xFFFFFFFF - val);
	lapic_timer_start = ((diff / freq) * 10);
	if(lapic_timer_start < 16) lapic_timer_start = 16; /* sanity */
	printk(0, "initial count = %d\n", lapic_timer_start);
	set_lapic_timer(lapic_timer_start);
}

void init_lapic(int extint)
{
	if(!(kernel_state_flags & KSF_CPUS_RUNNING))
		return;
	int i;
	/* we may be in a state where there are interrupts left
	 * in the registers that haven't been EOI'd. I'm pretending like
	 * I know why that may be. Linux does this, and that's their
	 * explination */
	for(i=0;i<=255;i++)
		lapic_eoi();
	/* these are not yet configured */
	LAPIC_WRITE(LAPIC_DFR, 0xFFFFFFFF);
	LAPIC_WRITE(LAPIC_LDR, (LAPIC_READ(LAPIC_LDR)&0x00FFFFFF)|1);
	/* disable the timer while we set up */
	LAPIC_WRITE(LAPIC_LVTT, LAPIC_DISABLE);
	/* if we accept the extint stuff (the boot processor) we need to not
	 * mask, and set the proper flags for these entries.
	 * LVT1: NMI
	 * LVT0: extINT, level triggered
	 */
	LAPIC_WRITE(LAPIC_LVT1, 0x400 | (extint ? 0 : LAPIC_DISABLE)); //NMI
	LAPIC_WRITE(LAPIC_LVT0, 0x8700 | (extint ? 0 : LAPIC_DISABLE)); //external interrupts
	/* disable errors (can trigger while messing with masking) and performance
	 * counter, but also set a non-zero vector */
	LAPIC_WRITE(LAPIC_LVTE, 0xFF | LAPIC_DISABLE);
	LAPIC_WRITE(LAPIC_LVTPC, 0xFF | LAPIC_DISABLE);
	
	/* accept all priority levels */
	LAPIC_WRITE(LAPIC_TPR, 0);
	/* finally write to the spurious interrupt register to enable
	 * the interrupts */
	LAPIC_WRITE(LAPIC_SPIV, 0x0100 | 0xFF);
}

void init_ioapic()
{
	if(!num_ioapic)
		return;
	unsigned i, num=0;
	interrupt_set(0);
	interrupt_controller = 0;
	disable_pic();
	/* enable all discovered ioapics */
	for(i=0;i<num_ioapic;i++) {
		struct ioapic *l = &ioapic_list[i];
		printk(1, "[apic]: found ioapic at %x: ID %d\n", l->addr, l->id);
		num += program_ioapic(l);
	}
	if(!num)
		panic(0, "unable to initialize IOAPICs");
	interrupt_controller = IOINT_APIC;
}
#endif
