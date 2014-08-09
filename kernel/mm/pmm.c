#include <sea/mm/_mm.h>
#include <sea/config.h>
#include <sea/mm/pmm.h>
#include <sea/mm/dma.h>
#include <sea/boot/multiboot.h>
#include <sea/kernel.h>
#include <sea/tm/schedule.h>

volatile addr_t pm_location=0;
static volatile addr_t pm_stack = PM_STACK_ADDR;
static volatile addr_t pm_stack_max = PM_STACK_ADDR;

volatile unsigned long pm_num_pages=0, pm_used_pages=0;
volatile uint64_t highest_page=0;
volatile uint64_t lowest_page=~0;

static addr_t pmm_contiguous_address_start;
static int pmm_contiguous_end_index, pmm_contiguous_max_index, pmm_contiguous_index_bytes, pmm_contiguous_start_index;
static uint8_t pmm_contiguous_index[4096];

volatile addr_t placement;
mutex_t pm_mutex;

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

static addr_t mm_reclaim_page_from_contiguous()
{
	assert(mutex_is_locked(&pm_mutex));
	printk(0, "[pmm]: reclaiming a page from contiguous region (%d)\n", pmm_contiguous_end_index);
	/* test end index, if it's empty, mark it and return it. */
	int i = pmm_contiguous_end_index;
	if(i == pmm_contiguous_start_index) return 0;
	if(pmm_contiguous_index[i/8] & (1 << (i % 8))) {
		return 0;
	}
	pmm_contiguous_index[i/8] |= (1 << (i % 8));
	pmm_contiguous_end_index--;
	return i * PAGE_SIZE + pmm_contiguous_address_start;
}

static void mm_append_page_to_contiguous(addr_t ad)
{
	assert(mutex_is_locked(&pm_mutex));
	printk(0, "[pmm]: appending a page to contiguous region\n");
	int index = (ad - pmm_contiguous_address_start) / PAGE_SIZE;
	assert(ad >= pmm_contiguous_address_start);
	assert(index == pmm_contiguous_end_index+1);
	assert(index <= pmm_contiguous_max_index);
	assert(pmm_contiguous_index[index / 8] & (1 << (index%8)));

	pmm_contiguous_index[index/8] &= ~(1 << (index % 8));
	pmm_contiguous_end_index++;
}

static int mm_should_page_append_to_contiguous(addr_t ad)
{
	assert(mutex_is_locked(&pm_mutex));
	if(ad < pmm_contiguous_address_start)
		return 0;
	if(ad > (pmm_contiguous_address_start + pmm_contiguous_max_index*PAGE_SIZE))
		return 0;
	int index = (ad - pmm_contiguous_address_start) / PAGE_SIZE;
	if(index > pmm_contiguous_max_index || index != (pmm_contiguous_end_index+1))
		return 0;
	return 1;
}

void mm_pmm_register_contiguous_memory(addr_t region_start)
{
	memset(pmm_contiguous_index, 0, pmm_contiguous_index_bytes);
	pmm_contiguous_address_start = region_start;
	pmm_contiguous_index_bytes = ((CONFIG_CONTIGUOUS_MEMORY*1024*1024) / PAGE_SIZE) / 8;
	pmm_contiguous_max_index = pmm_contiguous_end_index = pmm_contiguous_index_bytes*8 -1;
	printk(0, "[pmm]: registered contiguous memory region of size %dMB (%d index bytes) at %x\n",
			CONFIG_CONTIGUOUS_MEMORY, pmm_contiguous_index_bytes, region_start);
}

void mm_pmm_init_contiguous(addr_t start)
{
	if(start > pmm_contiguous_address_start)
		pmm_contiguous_start_index = ((start - pmm_contiguous_address_start) / PAGE_SIZE);
	else
		pmm_contiguous_start_index = 0;
}

/* p must contain a size and specified alignment */
void mm_alloc_contiguous_region(struct mm_physical_region *p)
{
	/* perform a linear scan for enough space */
	int npages = ((p->size-1) / PAGE_SIZE) + 1;
	int i, j;
	int start=-1, end=-1;
	uint8_t *ptr = pmm_contiguous_index;
	p->address = 0;
	mutex_acquire(&pm_mutex);
	for(i=(pmm_contiguous_start_index/8);i<pmm_contiguous_index_bytes;i++) {
		for(j=(i == (pmm_contiguous_start_index/8) ? pmm_contiguous_start_index % 8 : 0);j<8;j++) {
			if((i*8 + j) >= pmm_contiguous_end_index)
				goto out;
			if((ptr[i] & (1 << j)) == 0)
			{
				if(start == -1)
				{
					start = i * 8 + j;
				}
				end = i * 8 + j + 1;
				if(end - start == npages) {
					/* found space */
					for(i=start;i<end;i++) {
						ptr[i/8] |= (1 << (i % 8));
					}
					p->address = start * PAGE_SIZE + pmm_contiguous_address_start;
					goto out;
				}
			} else {
				start = end = -1;
			}
		}
	}
out:
	mutex_release(&pm_mutex);
}

/* p must contain a size and an address */
void mm_free_contiguous_region(struct mm_physical_region *p)
{
	int npages = ((p->size-1) / PAGE_SIZE)+1;
	int start = (p->address - pmm_contiguous_address_start) / PAGE_SIZE;
	assert(start >= pmm_contiguous_start_index && start <= pmm_contiguous_end_index);
	uint8_t *ptr = pmm_contiguous_index;
	mutex_acquire(&pm_mutex);
	p->address = 0;
	int i;
	for(i=start;i<(start+npages);i++)
	{
		assert((i/8) < pmm_contiguous_index_bytes);
		ptr[i/8] &= ~(1 << (i % 8));
	}
	mutex_release(&pm_mutex);
}

static addr_t __oom_handler()
{
	addr_t ret = mm_reclaim_page_from_contiguous();
	if(!ret)
		panic(PANIC_MEM | PANIC_NOSYNC, "ran out of physical memory");
	return ret;
}

addr_t mm_alloc_physical_page()
{
	if(!pm_location)
		panic(PANIC_MEM | PANIC_NOSYNC, "Physical memory allocation before initilization");
	addr_t ret = 0;
	if(kernel_state_flags & KSF_MEMMAPPED)
	{
		mutex_acquire(&pm_mutex);
		/* if the stack is empty, OR the poping from the stack would make it
		 * empty (want to reserve one page for emergencies), OR
		 * the next address is invalid, then call OOM */
		if(pm_stack == PM_STACK_ADDR
				|| (pm_stack -= sizeof(addr_t)) == PM_STACK_ADDR
				|| *(addr_t *)(pm_stack) <= pm_location) {
			ret = __oom_handler();
		}
		if(!ret) {
			/* __oom_handler was not called, so we're good */
			ret = *(addr_t *)pm_stack;
			*(addr_t *)pm_stack = 0;
			++pm_used_pages;
		}
		mutex_release(&pm_mutex);
	} else {
		/* this isn't locked, because it is used before multitasking happens */
		ret = pm_location;
		pm_location+=PAGE_SIZE;
	}
	if(current_task) {
		current_task->allocated++;
		current_task->phys_mem_usage++;
	}
	assert(ret);
	return ret;
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
	assert(addr);
	if(current_task) {
		current_task->freed++;
		current_task->phys_mem_usage--;
	}
	mutex_acquire(&pm_mutex);
	/* if we can put this back in contiguous memory, that's fine. */
	if(mm_should_page_append_to_contiguous(addr))
	{
		mm_append_page_to_contiguous(addr);
	}
	else if(pm_stack_max <= pm_stack)
	{
		/* TODO: not sure if this is correct... */
		if(!(kernel_state_flags & KSF_MEMMAPPED))
			mm_vm_map(pm_stack_max, mm_alloc_physical_page(), PAGE_PRESENT | PAGE_WRITE, 0);
		else
			mm_vm_map(pm_stack_max, addr, PAGE_PRESENT | PAGE_WRITE, 0);
		memset((void *)pm_stack_max, 0, PAGE_SIZE);
		pm_stack_max += PAGE_SIZE;
		if(!(kernel_state_flags & KSF_MEMMAPPED)) goto add;
	} else {
		add:
		assert(*(addr_t *)(pm_stack) = addr);
		pm_stack += sizeof(addr_t);
		--pm_used_pages;
		assert(*(addr_t *)(pm_stack - sizeof(addr_t)) == addr);
	}
	mutex_release(&pm_mutex);
}

void mm_pm_init(addr_t start, struct multiboot *mboot)
{
	pm_location = (start + PAGE_SIZE) & PAGE_MASK;
}

int mm_allocate_dma_buffer(struct dma_region *d)
{
	mm_alloc_contiguous_region(&d->p);
	if(d->p.address == 0)
		return -1;
	addr_t offset = d->p.address - pmm_contiguous_address_start;
	assert((offset&PAGE_MASK) == offset);

	int npages = ((d->p.size-1) / PAGE_SIZE) + 1;
	for(int i = 0;i<npages;i++)
		mm_vm_map(CONTIGUOUS_VIRT_START + offset + i * PAGE_SIZE, d->p.address + i*PAGE_SIZE, PAGE_PRESENT | PAGE_WRITE, 0);
	d->v = CONTIGUOUS_VIRT_START + offset;
	return 0;
}

int mm_free_dma_buffer(struct dma_region *d)
{
	int npages = ((d->p.size-1) / PAGE_SIZE) + 1;
	addr_t offset = d->p.address - pmm_contiguous_address_start;
	assert((offset&PAGE_MASK) == offset);
	for(int i=0;i<npages;i++)
		mm_vm_unmap_only(CONTIGUOUS_VIRT_START + offset + i * PAGE_SIZE, 0);
	mm_free_contiguous_region(&d->p);
	d->p.address = d->v = 0;
	return 0;
}

