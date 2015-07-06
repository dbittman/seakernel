#include <sea/config.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/schedule.h>
#if CONFIG_SMP
void cpu_smp_task_idle(struct cpu *cpu)
{
	cpu->flags |= CPU_TASK;
	cpu->idle_thread->system = -1;
	cpu_interrupt_set(1);
	/* wait until we have tasks to run */
	for(;;) 
		tm_schedule();
}
#endif

int cpu_get_num_running_processors()
{
#if CONFIG_SMP
	return num_booted_cpus + 1;
#else
	return 1;
#endif
}

int cpu_get_num_halted_processors()
{
#if CONFIG_SMP
	return num_halted_cpus;
#else
	return 0;
#endif
}

int cpu_get_num_secondary_processors()
{
	return cpu_get_num_running_processors()-1;
}
