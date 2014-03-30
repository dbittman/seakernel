#include <sea/kernel.h>
#include <sea/cpu/processor.h>

void cpu_send_ipi(int dest, unsigned signal, unsigned flags)
{
	arch_cpu_send_ipi(dest, signal, flags);
}
