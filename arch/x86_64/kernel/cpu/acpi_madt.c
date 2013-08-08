#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <acpi.h>

















void acpi_madt_parse_processor(void *ent)
{
	
}

void acpi_madt_parse_ioapic(void *ent)
{
	
}

int parse_acpi_madt()
{
	struct {
		uint8_t type;
		uint8_t length;
	} *ent;
	kprintf("PARSE MADT\n");
	int length;
	void *ptr = acpi_get_table_data("APIC", &length);
	if(!ptr) return 0;
	
	uint32_t controller_address = *(uint32_t *)ptr;
	uint32_t flags = *(uint32_t *)((uint32_t *)ptr + 1);
	
	kprintf("%x %x\n", controller_address, flags);
	
	void *tmp = (void *)((addr_t)ptr + 8);
	while((addr_t)tmp < (addr_t)ptr+length)
	{
		ent = tmp;
		if(ent->type == 0)
			acpi_madt_parse_processor(ent);
		else if(ent->type == 1)
			acpi_madt_parse_ioapic(ent);
		tmp = (void *)((addr_t)tmp + ent->length);
	}
	
	
	return 1;
}

#endif
