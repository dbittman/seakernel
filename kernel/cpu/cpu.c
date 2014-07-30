#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/schedule.h>
#include <sea/kernel.h>
#include <sea/string.h>

cpu_t *primary_cpu=0;
#if CONFIG_SMP
cpu_t cpu_array[CONFIG_MAX_CPUS];
unsigned cpu_array_num=0;
#endif
cpu_t primary_cpu_data;

void cpu_reset()
{
	arch_cpu_reset();
}

void cpu_halt()
{
	arch_cpu_halt();
}

void cpu_processor_init_1()
{
	arch_cpu_processor_init_1();
}

void cpu_processor_init_2()
{
	arch_cpu_processor_init_2();
}

void cpu_early_init()
{
	arch_cpu_early_init();
}

void cpu_print_stack_trace(int num)
{
	arch_cpu_print_stack_trace(num);
}

void cpu_copy_fixup_stack(addr_t new, addr_t old, size_t len)
{
	arch_cpu_copy_fixup_stack(new, old, len);
}

#if CONFIG_SMP
cpu_t *cpu_get(unsigned id)
{
	for(unsigned int i=0;i<cpu_array_num;i++)
	{
		if(cpu_array[i].snum == id) return &cpu_array[i];
	}
	return 0;
}

cpu_t *cpu_add(cpu_t *c)
{
	if(cpu_array_num >= CONFIG_MAX_CPUS)
		return 0;
	memcpy(&cpu_array[cpu_array_num], c, sizeof(cpu_t));
	mutex_create((mutex_t *)&(cpu_array[cpu_array_num].lock), MT_NOSCHED);
	return &cpu_array[cpu_array_num++];
}
#endif

void cpu_timer_install(int freq)
{
	tm_set_current_frequency_indicator(freq);
	arch_cpu_timer_install(freq);
}
