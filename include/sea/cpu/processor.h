#ifndef __SEA_CPU_PROCESSOR_H
#define __SEA_CPU_PROCESSOR_H
#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/tables-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/tables-x86_64.h>
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

typedef struct __cpu_t__ {
	unsigned num;
	unsigned flags;
	cpuid_t cpuid;
	int apicid;
	volatile page_dir_t *kd;
	volatile addr_t kd_phys;
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

void cpu_smp_task_idle(task_t *me);
int cpu_get_num_running_processors();
#if CONFIG_SMP

cpu_t *cpu_get(int id);
cpu_t *cpu_add(cpu_t *c);

#endif

extern cpu_t *primary_cpu;
void copy_update_stack(addr_t old, addr_t, unsigned length);
#endif
