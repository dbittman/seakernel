#ifndef __CPU_X86_COMMON_H
#define __CPU_X86_COMMON_H

#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/asm/system.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/tables-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/tables-x86_64.h>
#endif

typedef struct {
	char manufacturer_string[13];
	int max_basic_input_val;
	int max_ext_input_val;
	int features_ecx, features_edx;
	int ext_features_ecx, ext_features_edx;
	char stepping, model, family, type; 
	char cache_line_size, logical_processors, lapic_id;
	char cpu_brand[49];
} cpuid_t;

struct arch_cpu {
	cpuid_t cpuid;
	gdt_entry_t gdt[NUM_GDT_ENTRIES];
	gdt_ptr_t gdt_ptr;
	idt_ptr_t idt_ptr;
	tss_entry_t tss;
};


#define APIC_BCAST_ID			0xFF
#define	APIC_VERSION(x)			((x) & 0xFF)
#define	APIC_MAXREDIR(x)		(((x) >> 16) & 0xFF)
#define	APIC_ID(x)				((x) >> 24)
#define APIC_VER_NEW			0x10

#define IOAPIC_REGSEL			0
#define IOAPIC_RW				0x10
#define	IOAPIC_ID				0
#define	IOAPIC_VER				1
#define	IOAPIC_REDIR			0x10

#define LAPIC_DISABLE           0x10000
#define LAPIC_ID				0x20
#define LAPIC_VER				0x30
#define LAPIC_TPR				0x80
#define LAPIC_APR				0x90
#define LAPIC_PPR				0xA0
#define LAPIC_EOI				0xB0
#define LAPIC_LDR				0xD0
#define LAPIC_DFR				0xE0
#define LAPIC_SPIV				0xF0
#define	LAPIC_SPIV_ENABLE_APIC	0x100
#define LAPIC_ISR				0x100
#define LAPIC_TMR				0x180
#define LAPIC_IRR				0x200
#define LAPIC_ESR				0x280
#define LAPIC_ICR				0x300
#define	LAPIC_ICR_DS_SELF		0x40000
#define	LAPIC_ICR_DS_ALLINC		0x80000
#define	LAPIC_ICR_DS_ALLEX		0xC0000
#define	LAPIC_ICR_TM_LEVEL		0x8000
#define	LAPIC_ICR_LEVELASSERT	0x4000
#define	LAPIC_ICR_STATUS_PEND	0x1000
#define	LAPIC_ICR_DM_LOGICAL	0x800
#define	LAPIC_ICR_DM_LOWPRI		0x100
#define	LAPIC_ICR_DM_SMI		0x200
#define	LAPIC_ICR_DM_NMI		0x400
#define	LAPIC_ICR_DM_INIT		0x500
#define	LAPIC_ICR_DM_SIPI		0x600
#define LAPIC_ICR_SHORT_DEST    0x0
#define LAPIC_ICR_SHORT_SELF    0x1
#define LAPIC_ICR_SHORT_ALL     0x2
#define LAPIC_ICR_SHORT_OTHERS  0x3

#define CPU_IPI_DEST_ALL LAPIC_ICR_SHORT_ALL
#define CPU_IPI_DEST_SELF LAPIC_ICR_SHORT_SELF
#define CPU_IPI_DEST_OTHERS LAPIC_ICR_SHORT_OTHERS

#define LAPIC_LVTT				0x320
#define LAPIC_LVTPC		       	0x340
#define LAPIC_LVT0				0x350
#define LAPIC_LVT1				0x360
#define LAPIC_LVTE				0x370
#define LAPIC_TICR				0x380
#define LAPIC_TCCR				0x390
#define LAPIC_TDCR				0x3E0

#define EBDA_SEG_ADDR			0x40E
#define BIOS_RESET_VECTOR		0x467
#define LAPIC_ADDR_DEFAULT		0xFEE00000uL
#define IOAPIC_ADDR_DEFAULT		0xFEC00000uL
#define CMOS_RESET_CODE			0xF
#define	CMOS_RESET_JUMP			0xa
#define CMOS_BASE_MEMORY		0x15

#define CMOS_WRITE_BYTE(x,y) cmos_write(x,y)
#define CMOS_READ_BYTE(x)    cmos_read(x)

#define CR0_EM          (1 << 2)
#define CR0_MP          (1 << 1)
#define CR4_OSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)

#define LAPIC_READ(x) lapic_read(x)
#define LAPIC_WRITE(x, y) lapic_write(x, y)

extern addr_t lapic_addr;
extern unsigned lapic_timer_start;
extern mutex_t ipi_mutex;

struct cpu;

void load_tables_ap(struct cpu *cpu);
void parse_cpuid(struct cpu *);
#if CONFIG_SMP

int boot_cpu(unsigned id);
void calibrate_lapic_timer(unsigned freq);
extern unsigned bootstrap;
void init_ioapic();
#endif /* CONFIG_SMP */

static inline void arch_cpu_jump(addr_t x)
{
	asm("jmp *%0"::"r"(x));
}

static inline void arch_cpu_halt(void)
{
	asm("hlt");
}

static inline void arch_cpu_pause(void)
{
	asm("pause");
}

#endif
