#include <sea/config.h>
#if CONFIG_SMP
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/cpu/processor.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
#include <sea/vsprintf.h>

unsigned num_cpus=0, num_booted_cpus=0, num_failed_cpus=0;
volatile unsigned num_halted_cpus=0;
int parse_acpi_madt();
int probe_smp(void)
{
	if(!parse_acpi_madt()) return 0;
	set_ksf(KSF_CPUS_RUNNING);
	printk(5, "[cpu]: CPU%s initialized (boot=%d, #APs=%d: ok)                    \n", num_cpus > 1 ? "s" : "", primary_cpu->snum, num_booted_cpus);
	return num_booted_cpus > 0;
}
#endif
