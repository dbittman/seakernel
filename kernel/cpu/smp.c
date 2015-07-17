#include <sea/config.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/vsprintf.h>
#if CONFIG_SMP
void cpu_smp_task_idle(struct cpu *cpu)
{
	tm_thread_raise_flag(current_thread, THREAD_KERNEL);
	cpu->flags |= CPU_RUNNING;
	printk(1, "[smp]: cpu %d ready\n", cpu->knum);
	cpu_interrupt_set(1);
	/* wait until we have tasks to run */
	for(;;) {
		if(__current_cpu->work.count > 0)
			workqueue_dowork(&__current_cpu->work);
		else
			tm_schedule();
	}
}
#endif

int cpu_get_num_running_processors(void)
{
#if CONFIG_SMP
	return num_booted_cpus + 1;
#else
	return 1;
#endif
}

int cpu_get_num_halted_processors(void)
{
#if CONFIG_SMP
	return num_halted_cpus;
#else
	return 0;
#endif
}

int cpu_get_num_secondary_processors(void)
{
	return cpu_get_num_running_processors()-1;
}

