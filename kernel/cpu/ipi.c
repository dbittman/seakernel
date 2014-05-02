#include <sea/kernel.h>
#include <sea/cpu/processor.h>

#if CONFIG_SMP
void cpu_send_ipi(int dest, unsigned signal, unsigned flags)
{
	arch_cpu_send_ipi(dest, signal, flags);
}
#endif
