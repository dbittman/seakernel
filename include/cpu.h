#ifndef CPU_H
#define CPU_H
typedef struct {
    char manufacturer_string[13];
    int max_basic_input_val;
    int max_ext_input_val;
    int features_ecx, features_edx;
    char stepping, model, family, type; 
    char cache_line_size, logical_processors, lapic_id;
    char cpu_brand[49];
} cpuid_t;

typedef struct __cpu_t__ {
	unsigned num;
	unsigned flags;
	cpuid_t cpuid;
	int apicid;
	volatile void *current;
	char stack[1024];
	struct __cpu_t__ *next, *prev;
} cpu_t;
int initAcpi(void);
void add_cpu(cpu_t *c);
#define CPU_UP      0x1
#define CPU_RUNNING 0x2
#define CPU_ERROR   0x4
#define CPU_SSE     0x8
#define CPU_FPU    0x10
#define CPU_PAGING 0x20
extern cpu_t primary_cpu;

void parse_cpuid(cpu_t *);
void init_sse(cpu_t *);
void setup_fpu(cpu_t *);
#define CR0_EM          (1 << 2)
#define CR0_MP          (1 << 1)
#define CR4_OSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)


#if CONFIG_SMP
/* The following definitions are taken from http://www.uruk.org/mps/ */
extern cpu_t *cpu_list;


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



#define IMPS_READ(x)	(*((volatile unsigned *) (x)))
#define IMPS_WRITE(x,y)	(*((volatile unsigned *) (x)) = (y))


#define IMPS_MAX_CPUS			APIC_BCAST_ID

#define IMPS_FPS_SIGNATURE	('_' | ('M'<<8) | ('P'<<16) | ('_'<<24))
#define IMPS_FPS_IMCRP_BIT	0x80
#define IMPS_FPS_DEFAULT_MAX	7

#define IMPS_CTH_SIGNATURE	('P' | ('C'<<8) | ('M'<<16) | ('P'<<24))

#define		IMPS_FLAG_ENABLED	1
#define IMPS_BCT_PROCESSOR		0
#define		IMPS_CPUFLAG_BOOT	2
#define IMPS_BCT_BUS			1
#define IMPS_BCT_IOAPIC			2
#define IMPS_BCT_IO_INTERRUPT		3
#define IMPS_BCT_LOCAL_INTERRUPT	4
#define		IMPS_INT_INT		0
#define		IMPS_INT_NMI		1
#define		IMPS_INT_SMI		2
#define		IMPS_INT_EXTINT		3

struct imps_cth
{
	unsigned sig;
	unsigned short base_length;
	unsigned char spec_rev;
	unsigned char checksum;
	char oem_id[8];
	char prod_id[12];
	unsigned oem_table_ptr;
	unsigned short oem_table_size;
	unsigned short entry_count;
	unsigned lapic_addr;
	unsigned short extended_length;
	unsigned char extended_checksum;
	char reserved[1];
};

struct imps_fps
{
	unsigned sig;
	struct imps_cth *cth_ptr;
	unsigned char length;
	unsigned char spec_rev;
	unsigned char checksum;
	unsigned char feature_info[5];
};

struct imps_processor
{
	unsigned char type;			/* must be 0 */
	unsigned char apic_id;
	unsigned char apic_ver;
	unsigned char flags;
	unsigned signature;
	unsigned features;
	char reserved[8];
};

struct imps_bus
{
	unsigned char type;			/* must be 1 */
	unsigned char id;
	char bus_type[6];
};

struct imps_ioapic
{
	unsigned char type;			/* must be 2 */
	unsigned char id;
	unsigned char ver;
	unsigned char flags;
	unsigned addr;
};

struct imps_interrupt
{
	unsigned char type;			/* must be 3 or 4 */
	unsigned char int_type;
	unsigned short flags;
	unsigned char source_bus_id;
	unsigned char source_bus_irq;
	unsigned char dest_apic_id;
	unsigned char dest_apic_intin;
};

void add_ioapic(struct imps_ioapic *ioapic);
void init_ioapic();
void lapic_eoi();
void init_pic();
extern unsigned imps_lapic_addr;
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
#define IMPS_LAPIC_READ(x)  (*((volatile unsigned *) (imps_lapic_addr+(x))))
#define IMPS_LAPIC_WRITE(x, y)   \
   (*((volatile unsigned *) (imps_lapic_addr+(x))) = (y))
#endif
#endif
