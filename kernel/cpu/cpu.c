#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/schedule.h>
#include <sea/string.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>

struct cpu *primary_cpu=0;
#if CONFIG_SMP
struct cpu cpu_array[CONFIG_MAX_CPUS];
unsigned cpu_array_num=0;
#endif
struct cpu primary_cpu_data;

void cpu_reset()
{
	arch_cpu_reset();
}

void cpu_processor_init_1()
{
	arch_cpu_processor_init_1();
}

void cpu_processor_init_2()
{
	arch_cpu_processor_init_2();
#if CONFIG_MODULES
	loader_do_add_kernel_symbol((addr_t)&primary_cpu, "primary_cpu");
	loader_add_kernel_symbol(cpu_interrupt_set);
	loader_add_kernel_symbol(cpu_interrupt_get_flag);
#if CONFIG_SMP
	loader_add_kernel_symbol(cpu_get);
	loader_add_kernel_symbol((addr_t)&cpu_array_num);
	loader_add_kernel_symbol((addr_t)&num_booted_cpus);
#endif
#endif
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
struct cpu *cpu_get(unsigned id)
{
	/* TODO: fix this FUCKING RETARDED nonsense */
	for(unsigned int i=0;i<cpu_array_num;i++)
	{
		if(cpu_array[i].snum == id) return &cpu_array[i];
	}
	return 0;
}

struct cpu *cpu_add(struct cpu *c)
{
	if(cpu_array_num >= CONFIG_MAX_CPUS)
		return 0;
	memcpy(&cpu_array[cpu_array_num], c, sizeof(struct cpu));
	mutex_create((mutex_t *)&(cpu_array[cpu_array_num].lock), MT_NOSCHED);
	return &cpu_array[cpu_array_num++];
}
#endif

void cpu_timer_install(int freq)
{
	tm_set_current_frequency_indicator(freq);
	arch_cpu_timer_install(freq);
}

void cpu_disable_preemption()
{
	int old = cpu_interrupt_set(0);
	add_atomic(&__current_cpu->preempt_disable, 1);
	cpu_interrupt_set(old);
}

void cpu_enable_preemption()
{
	int old = cpu_interrupt_set(0);
	sub_atomic(&__current_cpu->preempt_disable, 1);
	cpu_interrupt_set(old);
}

struct cpu *cpu_get_current(void)
{
	cpu_disable_preemption();
	struct cpu *cpu = current_thread->cpu;
	return cpu;
}

void cpu_put_current(struct cpu *cpu)
{
	cpu_enable_preemption();
}

