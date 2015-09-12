#include <sea/config.h>
#include <sea/mm/pmm.h>
#include <sea/mm/dma.h>
#include <sea/boot/multiboot.h>
#include <sea/kernel.h>
#include <sea/fs/kerfs.h>
#include <sea/vsprintf.h>
#include <sea/cpu/interrupt.h>
#include <stdatomic.h>
addr_t pm_location=0;
static addr_t* pm_stack = (addr_t *)PM_STACK_ADDR;
static addr_t pm_stack_max = PM_STACK_ADDR;

unsigned long pm_num_pages=0, pm_used_pages=0;
uint64_t highest_page=0;
uint64_t lowest_page=~0;

static addr_t pmm_contiguous_address_start;
static int pmm_contiguous_end_index, pmm_contiguous_max_index, pmm_contiguous_index_bytes, pmm_contiguous_start_index;
static uint8_t pmm_contiguous_index[4096];

addr_t placement;
mutex_t pm_mutex;

void arch_mm_copy_page_physical(addr_t src, addr_t dest);
void mm_copy_page_physical(addr_t src, addr_t dest)
{
#if CONFIG_ARCH == TYPE_ARCH_X86
	arch_mm_copy_page_physical(src, dest);
#else
	panic(0, "x86_64 copy page physical unimplemented");
#endif
}

void arch_mm_zero_page_physical(addr_t page);
void mm_zero_page_physical(addr_t page)
{
#if CONFIG_ARCH == TYPE_ARCH_X86
	arch_mm_zero_page_physical(page);
#else
	panic(0, "x86_64 zero page physical unimplemented");
#endif
}

addr_t pmm_buddy_allocate(size_t length);
extern addr_t initrd_start_page, initrd_end_page;
extern int debug_sections_start, debug_sections_end;
void pmm_buddy_deallocate(addr_t address);
addr_t mm_alloc_physical_page(void)
{
	if(!pm_location)
		panic(PANIC_MEM | PANIC_NOSYNC, "Physical memory allocation before initilization");
	addr_t ret = 0;
	if(kernel_state_flags & KSF_MEMMAPPED)
	{
		ret = pmm_buddy_allocate(PAGE_SIZE);
	} else {
		/* this isn't locked, because it is used before multitasking happens */
		ret = pm_location;
		pm_location+=PAGE_SIZE;
		while((pm_location >= initrd_start_page && pm_location <= initrd_end_page))
			pm_location += PAGE_SIZE;
	}
	assert(ret);
	return ret;
}

addr_t mm_physical_allocate(size_t length, bool clear)
{
	addr_t ret = pmm_buddy_allocate(length);
	if(clear)
		arch_mm_physical_memset((void *)ret, 0, length);
	return ret;
}

addr_t mm_physical_allocate_region(size_t length, bool clear, addr_t min, addr_t max)
{
	/* TODO: actually implement this */
	return mm_physical_allocate(length, clear);
}

void mm_physical_deallocate(addr_t address)
{
	pmm_buddy_deallocate(address);
}

void mm_free_physical_page(addr_t addr)
{
	if(!(kernel_state_flags & KSF_PAGING))
		panic(PANIC_MEM | PANIC_NOSYNC, "Called free page without paging environment");
	if(addr < pm_location || (((addr > highest_page) || addr < lowest_page)
		&& (kernel_state_flags & KSF_MEMMAPPED))) {
		panic(PANIC_MEM | PANIC_NOSYNC, "tried to free invalid physical address (%x)", addr);
		return;
	}
	pmm_buddy_deallocate(addr);
}

extern int kernel_end;
void mm_pm_init(void)
{
	pm_location = ((((addr_t)&kernel_end - MEMMAP_KERNEL_START) & ~(PAGE_SIZE-1)) + PAGE_SIZE + 0x100000 /* HACK */);
	while((pm_location >= initrd_start_page && pm_location <= initrd_end_page))
		pm_location += PAGE_SIZE;
}

/* TODO: specify maximum */
int mm_allocate_dma_buffer(struct dma_region *d)
{
	d->p.address = mm_physical_allocate(d->p.size, false);
	if(d->p.address == 0)
		return -1;
	addr_t offset = d->p.address - pmm_contiguous_address_start;
	assert((offset&PAGE_MASK) == offset);

	int npages = ((d->p.size-1) / PAGE_SIZE) + 1;
	for(int i = 0;i<npages;i++)
		mm_virtual_map(CONTIGUOUS_VIRT_START + offset + i * PAGE_SIZE, d->p.address + i*PAGE_SIZE, PAGE_PRESENT | PAGE_WRITE, 0x1000);
	d->v = CONTIGUOUS_VIRT_START + offset;
	return 0;
}

int mm_free_dma_buffer(struct dma_region *d)
{
	int npages = ((d->p.size-1) / PAGE_SIZE) + 1;
	addr_t offset = d->p.address - pmm_contiguous_address_start;
	assert((offset&PAGE_MASK) == offset);
	for(int i=0;i<npages;i++)
		mm_virtual_unmap(CONTIGUOUS_VIRT_START + offset + i * PAGE_SIZE);
	mm_physical_deallocate(d->p.address);
	d->p.address = d->v = 0;
	return 0;
}


