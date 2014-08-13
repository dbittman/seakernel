#include <sea/config.h>
#if CONFIG_SMP
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/cpu/processor.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/imps-x86.h>
#include <sea/cpu/cpu-x86.h>
#include <sea/cpu/x86msr.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
volatile int imps_release_cpus = 0;
char imcr_present=0;

static int imps_get_checksum(unsigned start, int length)
{
	unsigned sum = 0;
	while (length-- > 0) 
		sum += *((unsigned char *) (start++));
	return (sum&0xFF);
}

static void imps_add_processor(struct imps_processor *proc)
{
	printk(0, "add proc\n");
	num_cpus++;
	int apicid = proc->apic_id;
	cpu_t new_cpu;
	if (!(proc->flags & IMPS_FLAG_ENABLED))
		return;
	memset(&new_cpu, 0, sizeof(cpu_t));
	new_cpu.snum = apicid;
	new_cpu.flags=0;
	cpu_t *cp = cpu_add(&new_cpu);
	if (proc->flags & (IMPS_CPUFLAG_BOOT)) {
		primary_cpu = cp;
		return;
	}
	if(!cp)
	{
		printk(2, "[smp]: refusing to initialize CPU %d\n", apicid);
		return;
	}
	int re = boot_cpu(apicid, proc->apic_ver);
	if(!re) {
		cp->flags |= CPU_ERROR;
		num_failed_cpus++;
	} else
		num_booted_cpus++;
}

static int imps_read_config_table(unsigned start, int count, int do_)
{
	int n=0;
	while (count-- > 0) {
		switch (*((unsigned char *)start)) {
			case IMPS_BCT_PROCESSOR:
				if(do_) imps_add_processor((struct imps_processor *)start);
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

static int imps_bad_bios(struct imps_fps *fps_ptr)
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
		sum = imps_get_checksum((unsigned)local_cth_ptr,
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
			sum = (imps_get_checksum(((unsigned)local_cth_ptr)
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
	printk(1, "[smp]: intel multiprocessor spec 1.%d BIOS support detected\n", fps_ptr->spec_rev);
	if (imps_bad_bios(fps_ptr)) {
		printk(1, "[smp]: corrupted BIOS structures, disabling SMP support\n");
		return;
	}

	if (fps_ptr->feature_info[1] & IMPS_FPS_IMCRP_BIT) {
		str_ptr = "IMCR and PIC";
		imcr_present=1;
	} else
		str_ptr = "Virtual Wire";
	if (fps_ptr->cth_ptr)
		lapic_addr = local_cth_ptr->lapic_addr;
	else
		lapic_addr = LAPIC_ADDR_DEFAULT;
	printk(1, "[smp]: APIC config: \"%s mode\" local APIC address: 0x%x\n",
		      str_ptr, lapic_addr);
	if (lapic_addr != (read_msr(0x1b) & 0xFFFFF000)) {
		printk(1, "[smp]: inconsistent local APIC address, disabling SMP support\n");
		return;
	}
	
	if (fps_ptr->cth_ptr) {
		char str1[16], str2[16];
		memcpy(str1, local_cth_ptr->oem_id, 8);
		str1[8] = 0;
		memcpy(str2, local_cth_ptr->prod_id, 12);
		str2[12] = 0;
		printk(0, "[smp]: OEM id: %s  product id: %s\n", str1, str2);
		cth_start = ((unsigned) local_cth_ptr) + sizeof(struct imps_cth);
		cth_count = local_cth_ptr->entry_count;
	} else
		return;
	imps_read_config_table(cth_start, cth_count, 1);
}

int imps_scan_mptables(unsigned addr, unsigned len)
{
	unsigned end = addr+len;
	struct imps_fps *fps_ptr = (struct imps_fps *)addr;
	while((unsigned)fps_ptr < end) {
		if (fps_ptr->sig == IMPS_FPS_SIGNATURE
		 && fps_ptr->length == 1
		 && (fps_ptr->spec_rev == 1 || fps_ptr->spec_rev == 4)
		 && !imps_get_checksum((unsigned)fps_ptr, 16)) {
			imps_read_bios(fps_ptr);
			return 1;
		}
		fps_ptr++;
	}
	return 0;
}

#endif
