/* kmalloc.c: Copyright (c) 2010 Daniel Bittman
 * Defines wrapper functions for kmalloc, kfree and friends
 */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>

addr_t __mm_do_kmalloc_slab(size_t sz, char align);
void __mm_do_kfree_slab(void *ptr);
void __mm_slab_init(addr_t start, addr_t end);

static char kmalloc_name[128];
/* TODO: more granular locking inside slab */
static mutex_t km_m;

void kmalloc_init(void)
{
	strncpy(kmalloc_name, "slab", 128);
	mutex_create(&km_m, 0);
	__mm_slab_init(KMALLOC_ADDR_START, KMALLOC_ADDR_END);
}

/* TODO: remove a lot of the file-line stuff */
static addr_t do_kmalloc(size_t sz, char align, char *file, int line)
{
	mutex_acquire(&km_m);
	addr_t ret = __mm_do_kmalloc_slab(sz, align);
	mutex_release(&km_m);
	if(!ret || ret >= KMALLOC_ADDR_END || ret < KMALLOC_ADDR_START)
		panic(PANIC_MEM | PANIC_NOSYNC, "kmalloc returned impossible address");
	memset((void *)ret, 0, sz);
	return ret;
}

void *__kmalloc(size_t s, char *file, int line)
{
	return (void *)do_kmalloc(s, 0, file, line);
}

void *__kmalloc_a(size_t s, char *file, int line)
{
	return (void *)do_kmalloc(s, 1, file, line);
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
	addr_t ret = do_kmalloc(s, 1, file, line);
	mm_vm_get_map(ret, p, 0);
	*p += ret%PAGE_SIZE;
	return (void *)ret;
}

void kfree(void *pt)
{
	if(!pt) return;
	mutex_acquire(&km_m);
	__mm_do_kfree_slab(pt);
	mutex_release(&km_m);
}
