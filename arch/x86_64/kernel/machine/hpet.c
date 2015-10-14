#include <sea/cpu/acpi.h>
#include <sea/mm/pmap.h>
#include <sea/mm/vmm.h>
struct hpet_header
{
	uint8_t hardware_rev_id;
	uint8_t comparator_count:5;
	uint8_t counter_size:1;
	uint8_t reserved:1;
	uint8_t legacy_replacement:1;
	uint16_t vendorid;
	uint32_t pci_addr_data;
	uint64_t address;
	uint8_t hpet_number;
	uint16_t minimum_tick;
	uint8_t page_protection;
} __attribute__((packed));

static addr_t hpet_addr;
static uint32_t countperiod;
uint64_t arch_hpt_get_nanoseconds();
static int hpet_have = 0;
static uint64_t hpet_read64(int offset)
{
	return *(uint64_t *)(hpet_addr + offset);
}

static void hpet_write64(int offset, uint64_t data)
{
	*(uint64_t *)(hpet_addr + offset) = data;
}

void x86_hpet_init(void)
{
	int len;
	struct hpet_header *table = acpi_get_table_data("HPET", &len);
	if(!table)
		return;
	hpet_have = 1;
	hpet_addr = table->address + PHYS_PAGE_MAP;

	uint64_t x = hpet_read64(0);
	countperiod = (x >> 32);

	/* turn on the timer */
	uint64_t conf = hpet_read64(0x10);
	conf |= 1;
	hpet_write64(0x10, conf);
}

uint64_t arch_hpt_get_nanoseconds(void)
{
	if(!hpet_have) {
		return 0;
	}
	return (hpet_read64(0xF0) * countperiod) / 1000000;
}

