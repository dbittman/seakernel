/* Defines functions for physical memory */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
void arch_mm_physical_memset(void *addr, int c, size_t length)
{
	addr_t start = (addr_t)addr + PHYS_PAGE_MAP;
	memset((void *)start, c, length);
}

void arch_mm_physical_memcpy(void *dest, void *src, size_t length, int mode)
{
	addr_t startd = (addr_t)dest;
	if(mode == PHYS_MEMCPY_MODE_DEST || mode == PHYS_MEMCPY_MODE_BOTH)
		startd += PHYS_PAGE_MAP;
	addr_t starts = (addr_t)src;
	if(mode == PHYS_MEMCPY_MODE_SRC || mode == PHYS_MEMCPY_MODE_BOTH)
		starts += PHYS_PAGE_MAP;
	memcpy((void *)startd, (void *)starts, length);
}


