#include <types.h>
#include <kernel.h>
#include <cpu.h>
#include <cpu-x86_64.h>
#include <task.h>
#include <mutex.h>
#include <atomic.h>
#include <mod.h>
cpu_t *primary_cpu=0;
#if CONFIG_SMP
cpu_t cpu_array[CONFIG_MAX_CPUS];
unsigned cpu_array_num=0;
#endif
cpu_t primary_cpu_data;

extern mutex_t ipi_mutex;
int set_int(unsigned new)
{
	/* need to make sure we don't get interrupted... */
	asm("cli");
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	unsigned old = cpu ? cpu->flags&CPU_INTER : 0;
	if(!new) {
		asm("cli");
		if(cpu) cpu->flags &= ~CPU_INTER;
	} else if(!cpu || cpu->flags&CPU_RUNNING) {
		asm("sti");
		if(cpu) cpu->flags |= CPU_INTER;
	}
	return old;
}
