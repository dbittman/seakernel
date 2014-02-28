#ifndef CPU_H
#define CPU_H
#include <types.h>
#include <memory.h>
#include <tqueue.h>
#include <task.h>
#include <mutex.h>
#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <cpu-x86.h>
  #include <tables-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <cpu-x86_64.h>
  #include <tables-x86_64.h>
#endif

#include <sea/cpu/processor.h>

void load_tables_ap(cpu_t *cpu);
cpu_t *add_cpu(cpu_t *c);
extern cpu_t *primary_cpu;
extern cpu_t cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;
extern volatile unsigned num_halted_cpus;
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
