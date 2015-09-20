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
	slab_init(MEMMAP_KMALLOC_START, MEMMAP_KMALLOC_END);
	valloc_create(&virtpages, MEMMAP_VIRTPAGES_START, MEMMAP_VIRTPAGES_END, PAGE_SIZE, 0);
}

/* TODO: remove a lot of the file-line stuff */
static addr_t do_kmalloc(size_t sz, char align, char *file, int line)
{
	if(current_thread && current_thread->interrupt_level)
		panic(PANIC_NOSYNC, "cannot allocate memory within interrupt context");
	if(sz > 0x80000) {
		struct valloc_region reg;
		int np = (sz-1) / PAGE_SIZE + 1;
		void *test = valloc_allocate(&virtpages, &reg, np);
		if(!test)
			panic(PANIC_NOSYNC, "could not allocate large region");
		for(addr_t a = reg.start;a < reg.start + np * PAGE_SIZE;a+=PAGE_SIZE)
			mm_virtual_trymap(a, PAGE_PRESENT | PAGE_WRITE, PAGE_SIZE);
		memset((void *)reg.start, 0, sz);
		return reg.start;
	}
	addr_t ret;
	ret = (addr_t)slab_kmalloc(sz);
	if(!ret || ret >= MEMMAP_KMALLOC_END || ret < MEMMAP_KMALLOC_START)
		panic(PANIC_MEM | PANIC_NOSYNC, "kmalloc returned impossible address %x", ret);
	memset((void *)ret, 0, sz);
	return ret;
}

void *__kmalloc(size_t s, char *file, int line)
{
	return (void *)do_kmalloc(s, 0, file, line);
}

/* TODO: do we ever use this? */
void *__kmalloc_a(size_t s, char *file, int line)
{
	assert(s == PAGE_SIZE);
	struct valloc_region reg;
	void *test = valloc_allocate(&virtpages, &reg, 1);
	if(!test) {
		panic(PANIC_NOSYNC, "could not allocate aligned region (out of memory?)");
	}
	/* NOTE: we don't need to lock this operation because
	 * allocations cannot share physical pages. */
	mm_virtual_trymap(reg.start, PAGE_PRESENT | PAGE_WRITE, mm_page_size(0));

	memset((void *)reg.start, 0, PAGE_SIZE);
	return (void *)reg.start;
}

void *__kmalloc_p(size_t s, addr_t *p, char *file, int line)
{
	addr_t ret = do_kmalloc(s, 0, file, line);
	mm_virtual_getmap(ret, p, NULL);
	*p += ret%PAGE_SIZE;
	return (void *)ret;
}

void *__kmalloc_ap(size_t s, addr_t *p, char *file, int line)
{
	addr_t ret = (addr_t)__kmalloc_a(s, file, line);
	mm_virtual_getmap(ret, p, NULL);
	*p += ret%PAGE_SIZE;
	return (void *)ret;
}

void kfree(void *pt)
{
	if(current_thread && current_thread->interrupt_level)
		panic(PANIC_NOSYNC, "cannot free memory within interrupt context");
	addr_t address = (addr_t)pt;
	if(address >= MEMMAP_VIRTPAGES_START && address < MEMMAP_VIRTPAGES_END) {
		struct valloc_region reg;
		reg.flags = 0;
		reg.npages = 1;
		reg.start = address;
		assert((address & ~PAGE_MASK) == 0);
		valloc_deallocate(&virtpages, &reg);
	} else {
		assert(address >= MEMMAP_KMALLOC_START && address < MEMMAP_KMALLOC_END);
		slab_kfree(pt);
	}
}

