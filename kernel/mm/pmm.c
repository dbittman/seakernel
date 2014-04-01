#include <sea/mm/_mm.h>
#include <sea/config.h>
#include <sea/mm/pmm.h>
#include <sea/mm/dma.h>

void mm_copy_page_physical(addr_t src, addr_t dest)
{
#if CONFIG_ARCH == TYPE_ARCH_X86
	arch_mm_copy_page_physical(src, dest);
#else
	panic(0, "x86_64 copy page physical unimplemented");
#endif
}

void mm_zero_page_physical(addr_t page)
{
#if CONFIG_ARCH == TYPE_ARCH_X86
	arch_mm_zero_page_physical(page);
#else
	panic(0, "x86_64 zero page physical unimplemented");
#endif
}

addr_t mm_alloc_physical_page()
{
	return arch_mm_alloc_physical_page();
}

void mm_free_physical_page(addr_t page)
{
	arch_mm_free_physical_page(page);
}

void mm_pm_init()
{
	arch_mm_pm_init();
}

int mm_allocate_dma_buffer(size_t length, addr_t *x, addr_t *physical)
{
	return arch_mm_allocate_dma_buffer(length, x, physical);
}
