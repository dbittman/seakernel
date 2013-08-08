#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>
#include <atomic.h>
#include <imps-x86_64.h>

unsigned num_cpus=0, num_booted_cpus=0, num_failed_cpus=0;
int imps_scan_mptables(unsigned addr, unsigned len);
int parse_acpi_madt();
int probe_smp()
{
	if(!parse_acpi_madt()) return 0;
	set_ksf(KSF_CPUS_RUNNING);
	printk(5, "[cpu]: CPU%s initialized (boot=%d, #APs=%d: ok)                    \n", num_cpus > 1 ? "s" : "", primary_cpu->apicid, num_booted_cpus);
	return num_booted_cpus > 0;
}
#endif
