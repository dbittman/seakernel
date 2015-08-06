/* kmalloc.c: Copyright (c) 2010 Daniel Bittman
 * Defines wrapper functions for kmalloc, kfree and friends
 */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>

void slab_kfree(void *data);
void *slab_kmalloc(size_t __size);
void slab_init(addr_t start, addr_t end);
struct valloc virtpages;

static char kmalloc_name[128];

void kmalloc_init(void)
{
	strncpy(kmalloc_name, "slab", 128);
	slab_init(KMALLOC_ADDR_START, KMALLOC_ADDR_END);
	valloc_create(&virtpages, VIRTPAGES_START, VIRTPAGES_END, PAGE_SIZE, 0);
}

/* TODO: remove a lot of the file-line stuff */
static addr_t do_kmalloc(size_t sz, char align, char *file, int line)
{
	if(current_thread && current_thread->interrupt_level)
		panic(PANIC_NOSYNC, "cannot allocate memory within interrupt context");
	addr_t ret;
	ret = (addr_t)slab_kmalloc(sz);
	if(!ret || ret >= KMALLOC_ADDR_END || ret < KMALLOC_ADDR_START)
		panic(PANIC_MEM | PANIC_NOSYNC, "kmalloc returned impossible address %x", ret);
	memset((void *)ret, 0, sz);
	return ret;
}

void *__kmalloc(size_t s, char *file, int line)
{
	return (void *)do_kmalloc(s, 0, file, line);
}

void *__kmalloc_a(size_t s, char *file, int line)
{
	assert(s == PAGE_SIZE);
	struct valloc_region reg;
	assert(valloc_allocate(&virtpages, &reg, 1));
	/* NOTE: we don't need to lock this operation because
	 * allocations cannot share physical pages. */
	map_if_not_mapped(reg.start);
	memset((void *)reg.start, 0, PAGE_SIZE);
	return (void *)reg.start;
}

void *__kmalloc_p(size_t s, addr_t *p, char *file, int line)
{
	addr_t ret = do_kmalloc(s, 0, file, line);
	mm_vm_get_map(ret, p, 0);
	*p += ret%PAGE_SIZE;
	return (void *)ret;
}

void *__kmalloc_ap(size_t s, addr_t *p, char *file, int line)
{
	addr_t ret = (addr_t)__kmalloc_a(s, file, line);
	mm_vm_get_map(ret, p, 0);
	*p += ret%PAGE_SIZE;
	return (void *)ret;
}

void kfree(void *pt)
{
	if(current_thread && current_thread->interrupt_level)
		panic(PANIC_NOSYNC, "cannot free memory within interrupt context");
	addr_t address = (addr_t)pt;
	if(address >= VIRTPAGES_START && address < VIRTPAGES_END) {
		struct valloc_region reg;
		reg.flags = 0;
		reg.npages = 1;
		reg.start = address;
		assert((address & ~PAGE_MASK) == 0);
		valloc_deallocate(&virtpages, &reg);
	} else {
		assert(address >= KMALLOC_ADDR_START && address < KMALLOC_ADDR_END);
		slab_kfree(pt);
	}
}

