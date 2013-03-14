#ifndef CPU_H
#define CPU_H
#include <memory.h>
#include <tqueue.h>
#include <task.h>
#include <mutex.h>
#include <tables.h>
typedef struct {
    char manufacturer_string[13];
    int max_basic_input_val;
    int max_ext_input_val;
    int features_ecx, features_edx;
    char stepping, model, family, type; 
    char cache_line_size, logical_processors, lapic_id;
    char cpu_brand[49];
} cpuid_t;
#define CPU_STACK_TEMP_SIZE 1024
typedef struct __cpu_t__ {
	unsigned num;
	unsigned flags;
	cpuid_t cpuid;
	int apicid;
	page_dir_t *kd;
	addr_t kd_phys;
	tqueue_t *active_queue;
	task_t *ktask, *cur;
	mutex_t lock;
	gdt_entry_t gdt[6];
	gdt_ptr_t gdt_ptr;
	tss_entry_t tss;
	unsigned stack[CPU_STACK_TEMP_SIZE];
	struct __cpu_t__ *next, *prev;
} cpu_t;
int initAcpi(void);
cpu_t *add_cpu(cpu_t *c);
#define CPU_UP      0x1
#define CPU_RUNNING 0x2
#define CPU_ERROR   0x4
#define CPU_SSE     0x8
#define CPU_FPU    0x10
#define CPU_PAGING 0x20
#define CPU_INTER  0x40
#define CPU_TASK   0x80
extern cpu_t *primary_cpu;
extern cpu_t cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;
void parse_cpuid(cpu_t *);
void init_sse(cpu_t *);
void setup_fpu(cpu_t *);
#define CR0_EM          (1 << 2)
#define CR0_MP          (1 << 1)
#define CR4_OSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)


#if CONFIG_SMP
/* The following definitions are taken from http://www.uruk.org/mps/ */
extern unsigned num_cpus, num_booted_cpus, num_failed_cpus;
int boot_cpu(unsigned id, unsigned apic_ver);
void calibrate_lapic_timer(unsigned freq);
extern char smp_enabled;
#define APIC_BCAST_ID			       	0xFF
#define	APIC_VERSION(x)				((x) & 0xFF)
#define	APIC_MAXREDIR(x)			(((x) >> 16) & 0xFF)
#define	APIC_ID(x)				((x) >> 24)
#define APIC_VER_NEW				0x10

#define IOAPIC_REGSEL				0
#define IOAPIC_RW				0x10
#define		IOAPIC_ID			0
#define		IOAPIC_VER			1
#define		IOAPIC_REDIR			0x10

#define LAPIC_ID				0x20
#define LAPIC_VER				0x30
#define LAPIC_TPR				0x80
#define LAPIC_APR				0x90
#define LAPIC_PPR				0xA0
#define LAPIC_EOI				0xB0
#define LAPIC_LDR				0xD0
#define LAPIC_DFR				0xE0
#define LAPIC_SPIV				0xF0
#define		LAPIC_SPIV_ENABLE_APIC		0x100
#define LAPIC_ISR				0x100
#define LAPIC_TMR				0x180
#define LAPIC_IRR				0x200
#define LAPIC_ESR				0x280
#define LAPIC_ICR				0x300
#define		LAPIC_ICR_DS_SELF		0x40000
#define		LAPIC_ICR_DS_ALLINC		0x80000
#define		LAPIC_ICR_DS_ALLEX		0xC0000
#define		LAPIC_ICR_TM_LEVEL		0x8000
#define		LAPIC_ICR_LEVELASSERT		0x4000
#define		LAPIC_ICR_STATUS_PEND		0x1000
#define		LAPIC_ICR_DM_LOGICAL		0x800
#define		LAPIC_ICR_DM_LOWPRI		0x100
#define		LAPIC_ICR_DM_SMI		0x200
#define		LAPIC_ICR_DM_NMI		0x400
#define		LAPIC_ICR_DM_INIT		0x500
#define		LAPIC_ICR_DM_SIPI		0x600
#define LAPIC_LVTT				0x320
#define LAPIC_LVTPC		       		0x340
#define LAPIC_LVT0				0x350
#define LAPIC_LVT1				0x360
#define LAPIC_LVTE				0x370
#define LAPIC_TICR				0x380
#define LAPIC_TCCR				0x390
#define LAPIC_TDCR				0x3E0


void init_pic();
int send_ipi(unsigned int dst, unsigned int v);

extern unsigned bootstrap;
#define EBDA_SEG_ADDR			0x40E
#define BIOS_RESET_VECTOR		0x467
#define LAPIC_ADDR_DEFAULT		0xFEE00000uL
#define IOAPIC_ADDR_DEFAULT		0xFEC00000uL
#define CMOS_RESET_CODE			0xF
#define	CMOS_RESET_JUMP			0xa
#define CMOS_BASE_MEMORY		0x15
unsigned char readCMOS(unsigned char addr);
void writeCMOS(unsigned char addr, unsigned int value);
#define CMOS_WRITE_BYTE(x,y) writeCMOS(x,y)
#define CMOS_READ_BYTE(x) readCMOS(x)
cpu_t *get_cpu(int id);

#endif
#endif
