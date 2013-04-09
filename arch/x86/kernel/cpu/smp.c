#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>
#include <atomic.h>
#include <imps.h>

unsigned num_cpus=0, num_booted_cpus=0, num_failed_cpus=0;
char smp_enabled=0;
extern int imps_enabled;
int send_ipi(unsigned int dst, unsigned int v)
{
	int to, send_status;
	cli();
	IMPS_LAPIC_WRITE(LAPIC_ICR+0x10, (dst << 24));
	IMPS_LAPIC_WRITE(LAPIC_ICR, v);

	/* Wait for send to finish */
	to = 0;
	do {
		delay_sleep(1);
		cli();
		send_status = IMPS_LAPIC_READ(LAPIC_ICR) & LAPIC_ICR_STATUS_PEND;
	} while (send_status && (to++ < 1000));
	return (to < 1000);
}

int probe_smp()
{
	unsigned mem_lower = ((CMOS_READ_BYTE(CMOS_BASE_MEMORY+1) << 8) | CMOS_READ_BYTE(CMOS_BASE_MEMORY)) << 10;
	int res=0;
	if(mem_lower < 512*1024 || mem_lower > 640*1024)
		return 0;
	if((unsigned)EBDA_SEG_ADDR > mem_lower - 1024 || (unsigned)EBDA_SEG_ADDR + *((unsigned char *)EBDA_SEG_ADDR) * 1024 > mem_lower)
		res=imps_scan_mptables(mem_lower - 1024, 1024);
	else
		res=imps_scan_mptables(EBDA_SEG_ADDR, 1024);
	if(!res)
		res=imps_scan_mptables(0xF0000, 0x10000);
	if(!res)
		return 0;
	imps_enabled = 1;
	printk(5, "[cpu]: CPU%s initialized (boot=%d, #APs=%d: ok)                    \n", num_cpus > 1 ? "s" : "", primary_cpu->apicid, num_booted_cpus);

	init_ioapic();
	init_lapic();
	calibrate_lapic_timer(1000);
	smp_enabled=1;
	return 0;
}
#endif
