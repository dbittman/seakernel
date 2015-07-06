#include <sea/types.h>
#include <sea/kernel.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/cpu/atomic.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/acpi.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/features-x86_common.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>

extern struct cpu primary_cpu_data;

void init_lapic(int);
void set_debug_traps (void);
int probe_smp();

void arch_cpu_processor_init_1()
{
#if CONFIG_SMP
	mutex_create(&ipi_mutex, MT_NOSCHED);
	memset(cpu_array, 0, sizeof(struct cpu) * CONFIG_MAX_CPUS);
	cpu_array_num = 0;
	int res = probe_smp();
	if(!(kernel_state_flags & KSF_CPUS_RUNNING))
		primary_cpu = &cpu_array[0];
	if(!primary_cpu)
		primary_cpu = &primary_cpu_data;
	load_tables_ap(primary_cpu);
	init_lapic(1);
	calibrate_lapic_timer(1000);
	init_ioapic();
 	if(res >= 0) 
		set_ksf(KSF_SMP_ENABLE);
	else
		kprintf("[smp]: error in init code, disabling SMP support\n");
#else
	primary_cpu = &primary_cpu_data;
	memset(primary_cpu, 0, sizeof(struct cpu));
	load_tables_ap(primary_cpu);
#endif
	assert(primary_cpu);
	cpu_interrupt_set(0);
	primary_cpu->flags = CPU_UP;
	printk(KERN_MSG, "Initializing CPU...\n");
	parse_cpuid(primary_cpu);
	x86_cpu_init_fpu(primary_cpu);
	x86_cpu_init_sse(primary_cpu);
	primary_cpu->flags |= CPU_RUNNING;
	printk(KERN_EVERY, "done\n");
	mutex_create((mutex_t *)&primary_cpu->lock, MT_NOSCHED);
#if CONFIG_GDB_STUB
	set_debug_traps();
	kprintf("---[DEBUG - waiting for GDB connection]---\n");
	__asm__("int $3");
	kprintf("---[DEBUG - resuming]---\n");
#endif
}

void x86_hpet_init();

void arch_cpu_processor_init_2()
{
	acpi_init();
	x86_hpet_init();
}
