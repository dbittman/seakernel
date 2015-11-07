#ifndef __SEA_MACHINE_BDA_X86_H
#define __SEA_MACHINE_BDA_X86_H

#include <stdint.h>
#include <sea/mm/vmm.h>
struct x86_bios_data_area {
	uint16_t com0;
	uint16_t com1;
	uint16_t com2;
	uint16_t com3;
	/* ...and then a whole lot of stuff we don't care about. */
};

static struct x86_bios_data_area *x86_bda = (void *)(0x400 + MEMMAP_KERNEL_START);

#endif

