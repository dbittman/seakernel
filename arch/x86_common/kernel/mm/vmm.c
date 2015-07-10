#include <sea/mm/vmm.h>

void arch_mm_flush_page_tables(void)
{
	flush_pd();
}

