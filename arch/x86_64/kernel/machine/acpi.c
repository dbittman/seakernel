/* functions for dealing with ACPI.
 * copyright (c) 2013 Daniel Bittman: This file is GPL'd.
 */
#include <sea/mm/vmm.h>
#include <sea/cpu/acpi.h>
#include <sea/loader/symbol.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
static int __acpi_enable = 0;
static int acpi_rsdt_pt_sz;
static struct acpi_dt_header *acpi_rsdt;

static int rsdp_validate_checksum(struct acpi_rsdp *rsdp)
{
	unsigned char *tmp = (unsigned char *)rsdp;
	unsigned sum=0;
	/* sum up the first 20 bytes */
	for(int i=0;i<20;i++)
	{
		sum += *tmp;
		tmp++;
	}
	if(sum&0xFF) return 0;
	if(rsdp->revision > 0)
	{
		/* validate the extended part of the table */
		sum=0;
		/* sum up the first 36 bytes */
		for(int i=0;i<36;i++)
		{
			sum += *tmp;
			tmp++;
		}
		if(sum&0xFF) return 0;
	}
	return 1;
}

static struct acpi_rsdp *apci_get_RSDP (void)
{
	/* The root structure may in the first KB of EBDA or from 0xE0000 to 0xFFFFF */
	addr_t ebda_bottom = *(uint16_t *)(0x40e + PHYS_PAGE_MAP) << 4;
	addr_t tmp = ebda_bottom;
	addr_t end = 0xFFFFF;
	struct acpi_rsdp *rsdp;
	if((0xA0000) < ebda_bottom || ((0xA0000 - ebda_bottom) > (128 * 1024))) {
		printk(0, "[acpi]: got invalid lower ebda address (%x)\n", ebda_bottom);
		tmp = 0xE0000;
		//return 0;
	}
	/* scan the EBDA and other region */
	while(tmp < end)
	{
		rsdp = (struct acpi_rsdp *)(tmp + PHYS_PAGE_MAP);
		if(!memcmp(rsdp->sig, "RSD PTR ", 8) && rsdp_validate_checksum(rsdp))
			return rsdp;
		tmp += 16;
		if(tmp >= 0xA0000 && tmp <0xE0000) tmp = 0xE0000;
	}
	return 0;
}

static int acpi_validate_dt(struct acpi_dt_header *sdt, const char *sig)
{
	if(strncmp(sdt->sig, sig, 4))
		return 0;
	unsigned char *tmp = (unsigned char *)sdt;
	unsigned sum=0;
	for(unsigned int i=0;i<sdt->length;i++)
	{
		sum += *tmp;
		tmp++;
	}
	return !(sum & 0xFF);
}

/* for RSDT pointer_size is 4, for XSDT pointer_size is 8 */
static addr_t find_RSDT_entry(struct acpi_dt_header *rsdt, int pointer_size, const char *sig)
{
	if(!__acpi_enable) return 0;
	addr_t tmp = (addr_t)(rsdt+1);
	int num_entries = (rsdt->length - sizeof(struct acpi_dt_header)) / pointer_size;
	for(int i=0;i<num_entries;i++)
	{
		addr_t v;
		if(pointer_size == 4)
			v = *(uint32_t *)(tmp);
		else
			v = *(uint64_t *)(tmp);
		v += PHYS_PAGE_MAP;
		char *test_sig = (char *)v;
		if(!strncmp(test_sig, sig, 4))
			return v;
		tmp += pointer_size;
	}
	return 0;
}

void *acpi_get_table_data(const char *sig, int *length)
{
	addr_t head = find_RSDT_entry(acpi_rsdt, acpi_rsdt_pt_sz, sig);
	if(!head)
		return (void *)0;
	struct acpi_dt_header *dt = (struct acpi_dt_header *)head;
	if(!acpi_validate_dt(dt, sig))
		return 0;
	if(length)
		*length = dt->length - sizeof(struct acpi_dt_header);
	return (dt+1);
}

void acpi_init(void)
{
	struct acpi_rsdp *rsdp = apci_get_RSDP();
	if(!rsdp) return;
	printk(0, "[acpi]: found valid RSDP structure at %x\n", rsdp);
	struct acpi_dt_header *rsdt = (struct acpi_dt_header *)((rsdp->revision ? (addr_t)rsdp->xsdt_addr : (addr_t)rsdp->rsdt_addr) + PHYS_PAGE_MAP);
	int pointer_size = (rsdp->revision ? 8 : 4);
	const char *sig = (rsdp->revision ? "XSDT" : "RSDT");
	int valid = acpi_validate_dt((void *)(rsdt), sig);
	
	acpi_rsdt = (void *)rsdt;
	acpi_rsdt_pt_sz = pointer_size;
	if(valid) __acpi_enable=1;
#if CONFIG_MODULES
	loader_add_kernel_symbol(acpi_get_table_data);
	loader_add_kernel_symbol(find_RSDT_entry);
#endif
}

