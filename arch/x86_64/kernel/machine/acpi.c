/* functions for dealing with ACPI.
 * copyright (c) 2013 Daniel Bittman: This file is GPL'd.
 */
#include <kernel.h>
#include <memory.h>
#include <acpi.h>

addr_t *__acpi_phys, *__acpi_virt;
int __acpi_idx=0;
int __acpi_idx_max=0;
int __acpi_enable = 0;
mutex_t __acpi_records_lock;
int acpi_rsdt_pt_sz;
struct acpi_dt_header *acpi_rsdt;
static addr_t get_virtual_address_page(addr_t p)
{
	addr_t masked = p & PAGE_MASK;
	for(int i=0;i<__acpi_idx;i++)
	{
		if(masked == __acpi_phys[i])
			return __acpi_virt[i];
	}
	if(__acpi_idx == __acpi_idx_max)
	{
		__acpi_idx_max *= 2;
		addr_t *tmp = kmalloc(__acpi_idx_max * sizeof(addr_t));
		memcpy(tmp, __acpi_phys, sizeof(addr_t) * __acpi_idx);
		tmp = kmalloc(__acpi_idx_max * sizeof(addr_t));
		memcpy(tmp, __acpi_virt, sizeof(addr_t) * __acpi_idx);
	}
	__acpi_phys[__acpi_idx] = masked;
	addr_t ret;
	__acpi_virt[__acpi_idx] = ret = get_next_mm_device_page();
	vm_map(ret, masked, PAGE_PRESENT | PAGE_WRITE, MAP_NOCLEAR);
	__acpi_idx++;
	return ret;
}

static addr_t translate_physical_address(addr_t p)
{
	int offset = (p - (p & PAGE_MASK));
	mutex_acquire(&__acpi_records_lock);
	addr_t v = get_virtual_address_page(p);
	mutex_release(&__acpi_records_lock);
	return v + offset;
}

int rsdp_validate_checksum(struct acpi_rsdp *rsdp)
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

struct acpi_rsdp *apci_get_RSDP()
{
	/* The root structure may in the first KB of EBDA or from 0xE0000 to 0xFFFFF */
	addr_t ebda_bottom = *(uint16_t *)(0x40e) << 4;
	addr_t tmp = ebda_bottom;
	addr_t end = 0xFFFFF;
	struct acpi_rsdp *rsdp;
	if(0xA0000 < ebda_bottom || ((0xA0000 - ebda_bottom) > (128 * 1024))) {
		printk(0, "[acpi]: got invalid lower ebda address (%x)\n", ebda_bottom);
		return 0;
	}
	/* scan the EBDA and other region */
	while(tmp < end)
	{
		rsdp = (struct acpi_rsdp *)tmp;
		if(!memcmp(rsdp->sig, "RSD PTR ", 8) && rsdp_validate_checksum(rsdp))
			return rsdp;
		tmp += 16;
		if(tmp >= 0xA0000 && tmp <0xE0000) tmp = 0xE0000;
	}
	return 0;
}

int acpi_validate_dt(struct acpi_dt_header *sdt, const char *sig)
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
addr_t find_RSDT_entry(struct acpi_dt_header *rsdt, int pointer_size, const char *sig)
{
	if(!__acpi_enable) return 0;
	addr_t tmp = (addr_t)(rsdt+1);
	int num_entries = (rsdt->length - sizeof(struct acpi_dt_header)) / pointer_size;
	for(int i=0;i<num_entries;i++)
	{
		addr_t v;
		if(pointer_size == 4)
			v = translate_physical_address(*(uint32_t *)tmp);
		else
			v = translate_physical_address(*(uint64_t *)tmp);
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
	if(!head) return (void *)0;
	struct acpi_dt_header *dt = (struct acpi_dt_header *)head;
	if(!acpi_validate_dt(dt, sig)) return 0;
	if(length) *length = dt->length - sizeof(struct acpi_dt_header);
	return (dt+1);
}

void init_acpi()
{
	__acpi_idx_max = 64;
	__acpi_phys = kmalloc(__acpi_idx_max * sizeof(addr_t));
	__acpi_virt = kmalloc(__acpi_idx_max * sizeof(addr_t));
	mutex_create(&__acpi_records_lock, 0);
	struct acpi_rsdp *rsdp = apci_get_RSDP();
	if(!rsdp) return;
	
	struct acpi_dt_header *rsdt = (struct acpi_dt_header *)(rsdp->revision ? rsdp->xsdt_addr : rsdp->rsdt_addr);
	int pointer_size = (rsdp->revision ? 8 : 4);
	const char *sig = (rsdp->revision ? "XSDT" : "RSDT");
	
	addr_t rsdt_v = translate_physical_address((addr_t)rsdt);
	int valid = acpi_validate_dt((void *)(rsdt_v), sig);
	acpi_rsdt = (void *)rsdt_v;
	acpi_rsdt_pt_sz = pointer_size;
	if(valid) __acpi_enable=1;
}
