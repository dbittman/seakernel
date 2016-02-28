#include <sea/config.h>
#if CONFIG_SMP
#include <sea/cpu/acpi.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/vsprintf.h>
#include <sea/string.h>

void acpi_madt_parse_processor(void *ent, int boot)
{
	struct {
		uint8_t type, length, acpi_processor_id, apicid;
		uint32_t flags;
	} *proc = ent;
	num_cpus++;
	if(!(proc->flags & 1))
	{
		printk(0, "[acpi]: detected disabled processor #%d (%d)\n", proc->apicid, proc->acpi_processor_id);
		return;
	}
	struct cpu *cp = 0;
	if(boot) {
		primary_cpu->snum = proc->apicid;
		return;
	} else{
		cp = cpu_add(proc->apicid);
		if(cp) cp->flags |= CPU_WAITING;
	}
	if(!cp)
	{
		printk(2, "[smp]: refusing to initialize CPU %d\n", proc->apicid);
		return;
	}
}

void acpi_madt_parse_ioapic(void *ent)
{
	struct {
		uint8_t type, length, apicid, __reserved;
		uint32_t address, int_start;
	} *st = ent;
	add_ioapic(st->address + PHYS_PAGE_MAP, st->apicid, st->int_start);
}

int parse_acpi_madt(void)
{
	int length;
	void *ptr = acpi_get_table_data("APIC", &length);
	if(!ptr) {
		printk(4, "[smp]: could not parse ACPI tables for multiprocessor information\n");
		return 0;
	}
	
	uint64_t controller_address = *(uint32_t *)ptr;
	lapic_addr = controller_address + PHYS_PAGE_MAP;
	void *tmp = (void *)((addr_t)ptr + 8);
	/* the ACPI MADT specification says that we may assume
	 * that the boot processor is the first processor listed
	 * in the table. */
	int boot_cpu = 1;
	while((addr_t)tmp < (addr_t)ptr+length)
	{
		struct {
			uint8_t type;
			uint8_t length;
		} *ent = tmp;
		if(ent->type == 0) {
			acpi_madt_parse_processor(ent, boot_cpu);
			boot_cpu = 0;
		}
		else if(ent->type == 1)
			acpi_madt_parse_ioapic(ent);
		tmp = (void *)((addr_t)tmp + ent->length);
	}
	return 1;
}

#endif
