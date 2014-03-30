#ifndef __X86_COMMON_ACPI_H
#define __X86_COMMON_ACPI_H

#include <sea/types.h>

__attribute__((packed)) struct acpi_rsdp {
	char sig[8];
	uint8_t checksum;
	char oemid[6];
	uint8_t revision;
	uint32_t rsdt_addr;
	uint32_t length;
	uint64_t xsdt_addr;
	uint8_t ex_checksum;
	char reserved[3];
};

__attribute__((packed)) struct acpi_dt_header {
	char sig[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oemid[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
};

#endif
