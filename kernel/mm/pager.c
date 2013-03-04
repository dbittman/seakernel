#include <kernel.h>
#include <memory.h>
#include <task.h>
unsigned tmp_page;
void __KT_pager()
{
	tmp_page = pm_alloc_page();
	for(;;) {
#if CONFIG_SWAP
		__KT_swapper();
#else
		//schedule();
#endif
	}
}
