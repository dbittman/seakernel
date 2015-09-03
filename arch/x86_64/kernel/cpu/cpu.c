#include <sea/types.h>
#include <sea/kernel.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>

#include <sea/cpu/acpi.h>
#include <sea/cpu/features-x86_common.h>
#include <sea/vsprintf.h>
#include <sea/cpu/interrupt.h>
#include <sea/string.h>
#include <sea/tty/terminal.h>
extern struct cpu *primary_cpu;
#if CONFIG_SMP
extern struct cpu cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;
#endif
extern struct cpu primary_cpu_data;
void init_lapic(int extint);
int probe_smp();
void init_acpi();
void arch_cpu_processor_init_1(void)
{
#if CONFIG_SMP
	primary_cpu = &cpu_array[0];
	primary_cpu->knum = 0;
	mutex_create(&ipi_mutex, MT_NOSCHED);
	memset(cpu_array, 0, sizeof(struct cpu) * CONFIG_MAX_CPUS);
	cpu_array_num = 1;
	load_tables_ap(primary_cpu);
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
	printk(KERN_EVERY, "done\n");
}

void arch_cpu_processor_init_2(void)
{
	acpi_init();
	x86_hpet_init();
#if CONFIG_SMP
	probe_smp();
	init_lapic(1);
	calibrate_lapic_timer(1000);
	init_ioapic();
	set_ksf(KSF_SMP_ENABLE);
#endif
}

#if CONFIG_SMP
int arch_cpu_boot_ap(struct cpu *cpu)
{
	int re = boot_cpu(cpu);
	if(!re) {
		cpu->flags |= CPU_ERROR;
		num_failed_cpus++;
	} else
		num_booted_cpus++;
	return re;
}
#endif

