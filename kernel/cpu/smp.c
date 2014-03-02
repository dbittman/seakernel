#include <config.h>
#if CONFIG_SMP
#include <task.h>
#include <cpu.h>

void cpu_smp_task_idle(task_t *me)
{
	cpu_t *cpu = me->cpu;
	cpu->flags |= CPU_TASK;
	me->system = -1;
	set_int(1);
	/* wait until we have tasks to run */
	for(;;) 
		tm_schedule();
}

#endif
