#ifndef __SEA_CPU_PROCESSOR_H
#define __SEA_CPU_PROCESSOR_H
#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/thread.h>
#include <sea/mutex.h>
#include <sea/tm/ticker.h>
#include <sea/tm/workqueue.h>

#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/cpu-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/cpu-x86_common.h>
#endif

#define CPU_STACK_TEMP_SIZE 256

#define CPU_UP      0x1
#define CPU_ERROR   0x2
#define CPU_WAITING 0x4

struct cpu {
	unsigned knum, snum; /* knum: cpu number to the kernel, snum: cpu number to the hardware */
	unsigned flags;
	cpuid_t cpuid;
	struct tqueue *active_queue;
	struct workqueue work;
	struct thread *idle_thread;
	unsigned numtasks;
	unsigned stack[CPU_STACK_TEMP_SIZE];
	struct ticker ticker;
	int preempt_disable;
	struct arch_cpu arch_cpu_data;
};

void cpu_smp_task_idle(struct cpu *me);
int cpu_get_num_running_processors();
int cpu_get_num_halted_processors();
int cpu_get_num_secondary_processors();

/* warning: this is not "safe" to use! The cpu could change on
 * any schedule, which may happen while you're using this. Use
 * cpu_get_current to make sure that doesn't happen. */
#define __current_cpu ((struct cpu *)current_thread->cpu)
extern struct cpu cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;

#if CONFIG_SMP

struct cpu *cpu_get(unsigned id);
struct cpu *cpu_add(int);
struct cpu *cpu_get_snum(unsigned id);

#endif

extern volatile unsigned num_halted_cpus;
extern unsigned num_cpus, num_booted_cpus, num_failed_cpus;

extern struct cpu *primary_cpu;
void arch_cpu_copy_fixup_stack(addr_t, addr_t, size_t length);
void cpu_copy_fixup_stack(addr_t, addr_t, size_t length);

void arch_cpu_send_ipi(int dest, unsigned signal, unsigned flags);
void cpu_send_ipi(int dest, unsigned signal, unsigned flags);

void arch_cpu_reset();
void cpu_reset();

void cpu_print_stack_trace(int num);
void arch_cpu_print_stack_trace(int num);

void cpu_processor_init_1();
void cpu_processor_init_2();

void arch_cpu_processor_init_1();
void arch_cpu_processor_init_2();

void arch_cpu_early_init();
void cpu_early_init();

static inline void cpu_halt(void)
{
	arch_cpu_halt();
}

static inline void cpu_pause(void)
{
	arch_cpu_pause();
}

static inline void cpu_jump(addr_t a)
{
	arch_cpu_jump(a);
}

struct cpu *cpu_get_current();
void cpu_put_current(struct cpu *);

void cpu_disable_preemption();
void cpu_enable_preemption();

void cpu_boot_all_aps(void);
#endif

