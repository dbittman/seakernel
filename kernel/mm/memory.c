#include <sea/mm/init.h>
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/boot/multiboot.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/loader/symbol.h>

#include <sea/mm/pmap.h>
#include <sea/mm/pmm.h>
#include <sea/mm/vmm.h>
#include <sea/mm/dma.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/reclaim.h>
#include <sea/vsprintf.h>
#include <sea/cpu/interrupt.h>
#include <stdbool.h>
struct vmm_context kernel_context;

extern addr_t initrd_start_page, initrd_end_page, kernel_start;
static inline bool __is_actually_free(addr_t x)
{
	if(x == 0)
		return false;
	if(x <= pm_location)
		return false;
	if(x >= initrd_start_page && x <= initrd_end_page)
		return false;
	return true;
}

static void process_memorymap(struct multiboot *mboot)
{
	addr_t i = mboot->mmap_addr;
	unsigned long num_pages=0, unusable=0;
	uint64_t j=0, address, length;
	int found_contiguous=0;
	lowest_page = ~0;
	while(i < (mboot->mmap_addr + mboot->mmap_length)){
		mmap_entry_t *me = (mmap_entry_t *)(i + MEMMAP_KERNEL_START);
		address = ((uint64_t)me->base_addr_high << 32) | (uint64_t)me->base_addr_low;
		length = (((uint64_t)me->length_high<<32) | me->length_low);
		if(me->type == 1 && length > 0)
		{
			for (j=address; 
				j < (address+length); j += PAGE_SIZE)
			{
				addr_t page;
#if ADDR_BITS == 32
				/* 32-bit can only handle the lower 32 bits of the address. If we're
				 * considering an address above 0xFFFFFFFF, we have to ignore it */
				page = (addr_t)(j & 0xFFFFFFFF); /*TODO CLEAN THIS UP */
				if((j >> 32) != 0)
					break;
#else
				page = j;
#endif
				if(__is_actually_free(page)) {
					if(lowest_page > page)
						lowest_page=page;
					if(page > highest_page)
						highest_page=page;
					num_pages++;
					pmm_buddy_deallocate(page);
				}
			}
		}
		i += me->size + sizeof (uint32_t);
	}
	printk(1, "[mm]: Highest page = %x, Lowest page = %x, num_pages = %d               \n", highest_page, lowest_page, num_pages);
	if(!j)
		panic(PANIC_MEM | PANIC_NOSYNC, "Memory map corrupted");
	int gbs=0;
	int mbs = ((num_pages * PAGE_SIZE)/1024)/1024;
	if(mbs < 4){
		console_kernel_puts("\n");
		panic(PANIC_MEM | PANIC_NOSYNC, 
				"Not enough memory, system wont work (%d MB, %d pages)", 
				mbs, num_pages);
	}
	gbs = mbs/1024;
	if(gbs > 0)
	{
		printk(KERN_MILE, "%d GB and ", gbs);
		mbs = mbs % 1024;
	}
	printk(KERN_MILE, "%d MB available memory (page size=%d KB, kmalloc=slab: ok)\n"
 			, mbs, PAGE_SIZE/1024);
	printk(1, "[mm]: num pages = %d\n", num_pages);
	pm_num_pages=num_pages;
	pm_used_pages=0;
	set_ksf(KSF_MEMMAPPED);
}

struct vmm_context kernel_context;
void mm_init(struct multiboot *m)
{
	printk(KERN_DEBUG, "[mm]: Setting up Memory Management...\n");
	mutex_create(&pm_mutex, MT_NOSCHED); /* allocating physical memory is required inside
										  * interrupt context because of page faults */
	mm_vm_init(&kernel_context);
	cpu_interrupt_register_handler (14, &arch_mm_page_fault_handle);
	pmm_buddy_init();
	process_memorymap(m);
	kmalloc_init();
	set_ksf(KSF_MMU);
	/* hey, look at that, we have happy memory times! */
	mm_reclaim_init();
#if CONFIG_MODULES
	loader_add_kernel_symbol(__kmalloc);
	loader_add_kernel_symbol(__kmalloc_ap);
	loader_add_kernel_symbol(__kmalloc_a);
	loader_add_kernel_symbol(__kmalloc_p);
	loader_add_kernel_symbol(kfree);
	loader_add_kernel_symbol(mm_vm_map);
	loader_add_kernel_symbol(pmap_get_mapping);
	loader_add_kernel_symbol(pmap_create);
	loader_add_kernel_symbol(pmap_destroy);
	loader_add_kernel_symbol(mm_alloc_physical_page);
	loader_add_kernel_symbol(mm_vm_get_map);
	loader_add_kernel_symbol(mm_vm_get_attrib);
	loader_add_kernel_symbol(mm_vm_set_attrib);
	loader_add_kernel_symbol(mm_free_physical_page);
	loader_add_kernel_symbol(mm_allocate_dma_buffer);
	loader_add_kernel_symbol(mm_free_dma_buffer);
	//loader_add_kernel_symbol(mm_alloc_contiguous_region);
	//loader_add_kernel_symbol(mm_free_contiguous_region);
#endif
}

int mm_stat_mem(struct mem_stat *s)
{
	if(!s) return -1;
	s->total = pm_num_pages * PAGE_SIZE;
	s->free = (pm_num_pages-pm_used_pages)*PAGE_SIZE;
	s->used = pm_used_pages*PAGE_SIZE;
	s->perc = ((float)pm_used_pages*100) / ((float)pm_num_pages);
	return 0;
}

