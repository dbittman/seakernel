#include <sea/config.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/schedule.h>
#if CONFIG_SMP
void cpu_smp_task_idle(task_t *me)
{
	cpu_t *cpu = me->cpu;
	cpu->flags |= CPU_TASK;
	me->system = -1;
	interrupt_set(1);
	/* wait until we have tasks to run */
	for(;;) 
		tm_schedule();
}
extern int num_booted_cpus;
#endif

int cpu_get_num_running_processors()
{
#if CONFIG_SMP
	return num_booted_cpus + 1;
#else
	return 1;
#endif
}
