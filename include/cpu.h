#ifndef CPU_H
#define CPU_H

#include <memory.h>
#include <tqueue.h>
#include <task.h>
#include <mutex.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <cpu-x86.h>
  #include <tables-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <cpu-x86_64.h>
  #include <tables-x86_64.h>
#endif

#define CPU_STACK_TEMP_SIZE 1024

#define CPU_UP      0x1
#define CPU_RUNNING 0x2
#define CPU_ERROR   0x4
#define CPU_SSE     0x8
#define CPU_FPU    0x10
#define CPU_PAGING 0x20
#define CPU_INTER  0x40
#define CPU_TASK   0x80
#define CPU_LOCK  0x100

typedef struct __cpu_t__ {
	unsigned num;
	unsigned flags;
	cpuid_t cpuid;
	int apicid;
	volatile page_dir_t *kd;
	addr_t kd_phys;
	tqueue_t *active_queue;
	task_t *ktask, *cur;
	mutex_t lock;
#if CONFIG_ARCH == TYPE_ARCH_X86 || CONFIG_ARCH == TYPE_ARCH_X86_64
	gdt_entry_t gdt[NUM_GDT_ENTRIES];
	gdt_ptr_t gdt_ptr;
	tss_entry_t tss;
#endif
	unsigned numtasks;
	unsigned stack[CPU_STACK_TEMP_SIZE];
	struct __cpu_t__ *next, *prev;
} cpu_t;

void load_tables_ap(cpu_t *cpu);
cpu_t *add_cpu(cpu_t *c);
extern cpu_t *primary_cpu;
extern cpu_t cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;
void parse_cpuid(cpu_t *);
void init_sse(cpu_t *);
void setup_fpu(cpu_t *);
void set_cpu_interrupt_flag(int flag);
int set_int(unsigned);
int get_cpu_interrupt_flag();
void init_pic();
unsigned char readCMOS(unsigned char addr);
void writeCMOS(unsigned char addr, unsigned int value);

#if CONFIG_SMP
/* The following definitions are taken from http://www.uruk.org/mps/ */
extern unsigned num_cpus, num_booted_cpus, num_failed_cpus;
int boot_cpu(unsigned id, unsigned apic_ver);
void calibrate_lapic_timer(unsigned freq);
int send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v);
extern unsigned bootstrap;
cpu_t *get_cpu(int id);
void init_ioapic();
void move_task_cpu(task_t *t, cpu_t *cpu);

#endif /* CONFIG_SMP */

#endif
