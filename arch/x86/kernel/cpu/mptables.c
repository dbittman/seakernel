#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>

int total_processors=0, tried_processors=0;
unsigned cpu_array, cpu_array_np=0;
unsigned bootstrap=0;
volatile int imps_release_cpus = 0;
int imps_enabled = 0;
int imps_num_cpus = 1;
unsigned imps_lapic_addr = 0;
char imcr_present=0;

static int
get_checksum(unsigned start, int length)
{
	unsigned sum = 0;
	while (length-- > 0) 
		sum += *((unsigned char *) (start++));
	return (sum&0xFF);
}

unsigned read_msr(uint32_t msr)
{
	unsigned res;
	asm("rdmsr":"=a"(res):"c"(msr));
	return res;
}

static int
send_ipi(unsigned int dst, unsigned int v)
{
	int to, send_status;
	cli();
	IMPS_LAPIC_WRITE(LAPIC_ICR+0x10, (dst << 24));
	IMPS_LAPIC_WRITE(LAPIC_ICR, v);

	/* Wait for send to finish */
	to = 0;
	do {
		delay_sleep(1);
		send_status = IMPS_LAPIC_READ(LAPIC_ICR) & LAPIC_ICR_STATUS_PEND;
	} while (send_status && (to++ < 1000));
	sti();
	return (to < 1000);
}
static int *booted = (int *)0x7200;
int TEST_BOOTED(unsigned a)
{
	return (*(unsigned *)(0x7200) == 0);
}
void load_tables_ap();
void set_lapic_timer(unsigned tmp);
extern unsigned lapic_timer_start;
void init_lapic();
void enable_A20();
void cpu_entry(void)
{
	int myid = *booted;
	cpu_t *cpu = get_cpu(myid);
	asm("mov %0, %%esp" : : "r" (cpu->stack + 1020));
	asm("mov %0, %%ebp" : : "r" (cpu->stack + 1020));
	enable_A20();
	load_tables_ap();
	myid = *booted;
	cpu = get_cpu(myid);
	parse_cpuid(cpu);
	setup_fpu(cpu);
	init_sse(cpu);
	init_lapic();
	cpu->flags |= CPU_UP;
	*booted=0;
	while(!mmu_ready);
	
	//kprintf("GOODMORNING MOTHERFUCKER: %x\n", cpu->kd_phys);
	__asm__ volatile ("mov %0, %%cr3" : : "r" (cpu->kd_phys));
	unsigned cr0temp;
	enable_paging();
	sti();
	/* well, now we wait for the scheduler to move us out of
	 * this endless loop and into a task. */
	while(!cpu->queue);
}

extern int bootmainloop(void);
extern int pmode_enter(void);
extern int RMGDT(void);
extern int GDTR(void);

static int boot_cpu(struct imps_processor *proc)
{
	int apicid = proc->apic_id, success = 1, to;
	unsigned bootaddr, accept_status;
	unsigned bios_reset_vector = BIOS_RESET_VECTOR;
	bootaddr = 0x7000;
	memcpy((char *)bootaddr, (void *)bootmainloop, 0x100);
	memcpy((char *)0x7150, (void *)RMGDT, 0x50);
	memcpy((char *)0x7100, (void *)GDTR, 0x50);
	memcpy((char *)0x7204, (void *)pmode_enter, (0x72FC) - 0x7204);
	*(unsigned *)(0x7200) = apicid;
	*(unsigned *)(0x72FC) = (unsigned)cpu_entry;
	/* set BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, CMOS_RESET_JUMP);
	*((volatile unsigned *) bios_reset_vector) = ((bootaddr & 0xFF000) << 12);

	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* assert INIT IPI */
	send_ipi(apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);
	delay_sleep(10);
	/* de-assert INIT IPI */
	send_ipi(apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);
	delay_sleep(10);
	if (proc->apic_ver >= APIC_VER_NEW) {
		int i;
		for (i = 1; i <= 2; i++) {
			send_ipi(apicid, LAPIC_ICR_DM_SIPI | ((bootaddr >> 12) & 0xFF));
			delay_sleep(1);
		}
	}
	to = 0;
	while (!TEST_BOOTED(bootaddr) && to++ < 100)
		delay_sleep(10);
	if (to >= 100) {
		printk(1, "\tCPU Not Responding");
		success = 0;
	}
	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;
	return success;
}

static void
add_processor(struct imps_processor *proc)
{
	int apicid = proc->apic_id;
	char str[64];
	sprintf(str, "[smp]: booting application processors (%d / %d)...\r", tried_processors++, total_processors-1);
	puts(str);
	printk(1, "[smp]: processor [APIC id %d ver %d]: ",
		      apicid, proc->apic_ver);
	if (!(proc->flags & IMPS_FLAG_ENABLED)) {
		printk(1, "DISABLED\n");
		return;
	}
	if (proc->flags & (IMPS_CPUFLAG_BOOT)) {
		bootstrap = apicid;
		printk(1, "\n[smp]:      found #0 (bootstrap Processor) - skipping\n");
		return;
	}
	if(!cpu_array_np)
	{
		cpu_array_np++;
		cpu_array = pm_alloc_page();
	}
	int alloc = (apicid-1)*sizeof(cpu_t) >= cpu_array_np*PAGE_SIZE;
	if(alloc) {
		pm_alloc_page();
		cpu_array_np++;
	}
	cpu_t *cpus = (cpu_t *)cpu_array;
	cpu_t *new_cpu = &cpus[apicid-1];
	memset(new_cpu, 0, sizeof(cpu_t));
	new_cpu->num=imps_num_cpus;
	new_cpu->apicid = apicid;
	new_cpu->flags=0;
	add_cpu(new_cpu);
	imps_num_cpus++;
	printk(1, "sending init interrupt...\n\t");
	int re = boot_cpu(proc);
	printk(1, "\n[smp]: processor [APIC id %d ver %d]: ",
		      apicid, proc->apic_ver);
	if (re) {
		printk(1, "BOOTED\n");
	} else {
		new_cpu->flags |= CPU_ERROR;
		printk(1, "FAILED\n");
	}
}

static int
imps_read_config_table(unsigned start, int count, int do_)
{
	int n=0;
	while (count-- > 0) {
		switch (*((unsigned char *)start)) {
			case IMPS_BCT_PROCESSOR:
				if(do_) add_processor((struct imps_processor *)start);
				start += 12;	/* 20 total */
				n++;
				break;
			case IMPS_BCT_BUS:
				//add_bus((imps_bus *)start);
				//kprintf("bus\n");
				break;
			case IMPS_BCT_IOAPIC:
				if(do_) add_ioapic((struct imps_ioapic *)start);
				break;
			default:
				break;
		}
		start += 8;
	}
	return n;
}

static int
imps_bad_bios(struct imps_fps *fps_ptr)
{
	int sum;
	struct imps_cth *local_cth_ptr
		= (struct imps_cth *) fps_ptr->cth_ptr;

	if (fps_ptr->feature_info[0] > IMPS_FPS_DEFAULT_MAX) {
		kprintf("    Invalid MP System Configuration type %d\n",
			      fps_ptr->feature_info[0]);
		return 1;
	}

	if (fps_ptr->cth_ptr) {
		sum = get_checksum((unsigned)local_cth_ptr,
                                   local_cth_ptr->base_length);
		if (local_cth_ptr->sig != IMPS_CTH_SIGNATURE || sum) {
			kprintf("    Bad MP Config Table sig 0x%x and/or checksum 0x%x\n", (unsigned)(fps_ptr->cth_ptr), sum);
			return 1;
		}
		if (local_cth_ptr->spec_rev != fps_ptr->spec_rev) {
			kprintf("    Bad MP Config Table sub-revision # %d\n", local_cth_ptr->spec_rev);
			return 1;
		}
		if (local_cth_ptr->extended_length) {
			sum = (get_checksum(((unsigned)local_cth_ptr)
					    + local_cth_ptr->base_length,
					    local_cth_ptr->extended_length)
			       + local_cth_ptr->extended_checksum) & 0xFF;
			if (sum) {
				kprintf("    Bad Extended MP Config Table checksum 0x%x\n", sum);
				return 1;
			}
		}
	} else if (!fps_ptr->feature_info[0]) {
		kprintf("    Missing configuration information\n");
		return 1;
	}

	return 0;
}

static void imps_read_bios(struct imps_fps *fps_ptr)
{
	int apicid;
	unsigned cth_start, cth_count;
	struct imps_cth *local_cth_ptr
		= (struct imps_cth *)fps_ptr->cth_ptr;
	char *str_ptr;
	printk(1, "[smp]: Intel MultiProcessor Spec 1.%d BIOS support detected\n", fps_ptr->spec_rev);
	if (imps_bad_bios(fps_ptr)) {
		printk(1, "[smp]: Disabling MPS support due to corrupted BIOS structures\n");
		return;
	}

	if (fps_ptr->feature_info[1] & IMPS_FPS_IMCRP_BIT) {
		str_ptr = "IMCR and PIC";
		imcr_present=1;
	} else
		str_ptr = "Virtual Wire";
	if (fps_ptr->cth_ptr)
		imps_lapic_addr = local_cth_ptr->lapic_addr;
	else
		imps_lapic_addr = LAPIC_ADDR_DEFAULT;
	printk(1, "[smp]: APIC config: \"%s mode\" Local APIC address: 0x%x\n",
		      str_ptr, imps_lapic_addr);
	if (imps_lapic_addr != (read_msr(0x1b) & 0xFFFFF000)) {
		printk(1, "[smp]: Inconsistent Local APIC address, Disabling SMP support\n");
		return;
	}
	
	apicid = IMPS_LAPIC_READ(LAPIC_SPIV);
	IMPS_LAPIC_WRITE(LAPIC_SPIV, apicid|LAPIC_SPIV_ENABLE_APIC);
	apicid = APIC_ID(IMPS_LAPIC_READ(LAPIC_ID));
	primary_cpu.apicid = apicid;
	if (fps_ptr->cth_ptr) {
		char str1[16], str2[16];
		memcpy(str1, local_cth_ptr->oem_id, 8);
		str1[8] = 0;
		memcpy(str2, local_cth_ptr->prod_id, 12);
		str2[12] = 0;
		printk(1, "[smp]: OEM id: %s  Product id: %s\n", str1, str2);
		cth_start = ((unsigned) local_cth_ptr) + sizeof(struct imps_cth);
		cth_count = local_cth_ptr->entry_count;
	} else
		return;
	total_processors = imps_read_config_table(cth_start, cth_count, 0);
	imps_enabled = 1;
	imps_read_config_table(cth_start, cth_count, 1);
}

int scan_mptables(unsigned addr, unsigned len)
{
	unsigned end = addr+len;
	struct imps_fps *fps_ptr = (struct imps_fps *)addr;
	while((unsigned)fps_ptr < end) {
		if (fps_ptr->sig == IMPS_FPS_SIGNATURE
		 && fps_ptr->length == 1
		 && (fps_ptr->spec_rev == 1 || fps_ptr->spec_rev == 4)
		 && !get_checksum((unsigned)fps_ptr, 16)) {
			imps_read_bios(fps_ptr);
			return 1;
		}
		fps_ptr++;
	}
	return 0;
}

void calibrate_lapic_timer(unsigned freq);

int probe_smp()
{
	unsigned mem_lower = ((CMOS_READ_BYTE(CMOS_BASE_MEMORY+1) << 8) | CMOS_READ_BYTE(CMOS_BASE_MEMORY)) << 10;
	int res=0;
	if(mem_lower < 512*1024 || mem_lower > 640*1024)
		return 0;
	if((unsigned)EBDA_SEG_ADDR > mem_lower - 1024 || (unsigned)EBDA_SEG_ADDR + *((unsigned char *)EBDA_SEG_ADDR) * 1024 > mem_lower)
		res=scan_mptables(mem_lower - 1024, 1024);
	else
		res=scan_mptables(EBDA_SEG_ADDR, 1024);
	if(!res)
		res=scan_mptables(0xF0000, 0x10000);
	if(!res)
		return 0;
	if(imps_enabled)
		printk(5, "[cpu]: CPU%s initialized (boot=%d, #APs=%d: ok)                    \n", imps_num_cpus > 1 ? "s" : "", bootstrap, imps_num_cpus-1);
	else
		printk(6, "[cpu]: Could not initialize application processors\n");
	
	init_ioapic();
	init_lapic();
	calibrate_lapic_timer(1000);
	return 0;
}
#endif
