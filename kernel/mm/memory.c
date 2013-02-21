#include <kernel.h>
#include <memory.h>
#include <multiboot.h>
#include <task.h>
#include <swap.h>
#include <cpu.h>
#include <elf.h>
extern unsigned int end;
void slab_stat(struct mem_stat *s);
void process_memorymap(struct multiboot *mboot)
{
	uint32_t i = mboot->mmap_addr;
	int num_pages=0;
	unsigned int j=0;
	while(i < (mboot->mmap_addr + mboot->mmap_length)){
		mmap_entry_t *me = (mmap_entry_t *)(i);
		printk(1, "[mm]: Map %d: %x -> %x\n", me->type, me->base_addr_low, 
				me->base_addr_low+me->length_low);
		if(me->type == 1)
		{
			for (j = me->base_addr_low; 
				j < (me->base_addr_low+me->length_low); j += PAGE_SIZE)
			{
				if(lowest_page > j)
					lowest_page=j;
				if(j > highest_page)
					highest_page=j;
				if(!(num_pages % 1000))
					printk(4, "Stacking memory: %d MB        \r", 
							(num_pages * 4) / 1024);
				if(j >= pm_location)
					pm_free_page(j);
				num_pages++;
			}
		}
		i += me->size + sizeof (uint32_t);
	}
	printk(1, "[mm]: Highest page = %x               \n", highest_page);
	if(!j)
		panic(PANIC_MEM | PANIC_NOSYNC, "Memory map corrupted");
	int gbs=0;
	int mbs = ((num_pages * PAGE_SIZE)/1024)/1024;
	if(mbs < 4){
		puts("\n");
		panic(PANIC_MEM | PANIC_NOSYNC, "Not enough memory, system wont work");
	}
	gbs = mbs/1024;
	if(gbs > 0)
	{
		printk(KERN_MILE, "%d GB and ", gbs);
		mbs = mbs % 1024;
	}
	printk(KERN_MILE, "[mm]: %d MB available memory (page size=0x1000, kmalloc=%s: ok)       \n"
			, mbs, KMALLOC_NAME);
	printk(1, "[mm]: num pages = %d\n", num_pages);
	pm_num_pages=num_pages;
	memory_has_been_mapped=1;
	pm_used_pages=0;
}

void init_memory(struct multiboot *m)
{
	printk(KERN_DEBUG, "[mm]: Setting up Memory Management...\n");
	mutex_create(&pm_mutex, 0);
	vm_init(pm_location);
	process_memorymap(m);
	install_kmalloc(KMALLOC_NAME, KMALLOC_INIT, KMALLOC_ALLOC, KMALLOC_FREE);
	vm_init_2();
	primary_cpu.flags |= CPU_PAGING;
	mmu_ready=1;
#if CONFIG_SWAP
	init_swap();
#endif
#if CONFIG_MODULES
	add_kernel_symbol(__kmalloc);
	add_kernel_symbol(kmalloc_ap);
	add_kernel_symbol(kmalloc_a);
	add_kernel_symbol(kmalloc_p);
	add_kernel_symbol(kfree);
	add_kernel_symbol(vm_map);
	add_kernel_symbol(vm_do_unmap);
	add_kernel_symbol(vm_do_unmap_only);
#endif
}

int pm_stat_mem(struct mem_stat *s)
{
	if(!s) return -1;
	s->total = pm_num_pages * PAGE_SIZE;
	s->free = (pm_num_pages-pm_used_pages)*PAGE_SIZE;
	s->used = pm_used_pages*PAGE_SIZE;
	s->perc = ((float)pm_used_pages*100) / ((float)pm_num_pages);
	slab_stat(s);
	return 0;
}
