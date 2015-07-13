#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/string.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/vsprintf.h>
#include <sea/tm/timing.h>

struct cpu *primary_cpu=0;
#if CONFIG_SMP
struct cpu cpu_array[CONFIG_MAX_CPUS];
unsigned cpu_array_num=0;
#endif
struct cpu primary_cpu_data;

void cpu_reset(void)
{
	arch_cpu_reset();
}

void cpu_processor_init_1(void)
{
	arch_cpu_processor_init_1();
}

void cpu_processor_init_2(void)
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

void cpu_early_init(void)
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

void cpu_set_kernel_stack(struct cpu *cpu, addr_t start, addr_t end)
{
	arch_cpu_set_kernel_stack(cpu, start, end);
}
#if CONFIG_SMP

struct cpu *cpu_get(unsigned id)
{
	return &cpu_array[id];
}

struct cpu *cpu_get_snum(unsigned id)
{
	for(unsigned int i=0;i<cpu_array_num;i++)
	{
		if(cpu_array[i].snum == id) return &cpu_array[i];
	}
	return 0;
}

struct cpu *cpu_add(int snum)
{
	if(cpu_array_num >= CONFIG_MAX_CPUS)
		return 0;
	cpu_array[cpu_array_num].snum = snum;
	cpu_array[cpu_array_num].knum = cpu_array_num;
	return &cpu_array[cpu_array_num++];
}

void cpu_boot_all_aps(void)
{
	for(unsigned i=0;i<cpu_array_num;i++) {
		struct cpu *cpu = &cpu_array[i];
		if(cpu->flags & CPU_WAITING) {
			cpu->flags &= ~CPU_WAITING;
			int res = arch_cpu_boot_ap(cpu);
			if(res < 0) {
				cpu->flags |= CPU_ERROR;
			}
		}
	}
	printk(2, "[smp]: initialized %d application CPUs\n", num_booted_cpus);
}

#endif

void cpu_timer_install(int freq)
{
	tm_set_current_frequency_indicator(freq);
	arch_cpu_timer_install(freq);
}

void cpu_disable_preemption(void)
{
	int old = cpu_interrupt_set(0);
	if(current_thread)
		add_atomic(&__current_cpu->preempt_disable, 1);
	cpu_interrupt_set(old);
}

void cpu_enable_preemption(void)
{
	int old = cpu_interrupt_set(0);
	if(current_thread)
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

