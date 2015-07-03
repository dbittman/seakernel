#ifndef __SEA_CPU_PROCESSOR_H
#define __SEA_CPU_PROCESSOR_H
#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/mutex.h>

#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/cpu-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/cpu-x86_common.h>
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
#define CPU_FXSAVE 0x200
#define CPU_NEEDRESCHED 0x400

struct cpu {
	unsigned knum, snum; /* knum: cpu number to the kernel, snum: cpu number to the hardware */
	unsigned flags;
	cpuid_t cpuid;
	volatile page_dir_t *kd;
	volatile addr_t kd_phys;
	struct tqueue *active_queue;
	struct thread *idle_thread, *current_thread /* TODO: do we need this? */;
	mutex_t lock;
	unsigned numtasks;
	unsigned stack[CPU_STACK_TEMP_SIZE];

	struct arch_cpu arch_cpu_data;

	struct __cpu_t__ *next, *prev;
};

void cpu_smp_task_idle(struct thread *me);
int cpu_get_num_running_processors();
int cpu_get_num_halted_processors();
int cpu_get_num_secondary_processors();

extern cpu_t cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;

#if CONFIG_SMP

cpu_t *cpu_get(unsigned id);
cpu_t *cpu_add(cpu_t *c);

#endif

extern volatile unsigned num_halted_cpus;
extern unsigned num_cpus, num_booted_cpus, num_failed_cpus;

extern cpu_t *primary_cpu;
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

static inline void cpu_halt()
{
	arch_cpu_halt();
}

static inline void cpu_pause()
{
	arch_cpu_pause();
}

static inline void cpu_jump(addr_t a)
{
	arch_cpu_jump(a);
}

#endif

