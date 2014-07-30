#include <sea/mm/vmm.h>

void arch_mm_flush_page_tables()
{
	flush_pd();
}

