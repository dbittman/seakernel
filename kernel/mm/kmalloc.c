/* kmalloc.c: Copyright (c) 2010 Daniel Bittman
 * Defines wrapper functions for kmalloc, kfree and friends
 */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
addr_t (*do_kmalloc_wrap)(size_t, char)=0;
void (*do_kfree_wrap)(void *)=0;
char kmalloc_name[128];
mutex_t km_m;
void kmalloc_create(char *name, unsigned (*init)(addr_t, addr_t), 
	addr_t (*alloc)(size_t, char), void (*free)(void *))
{
	do_kmalloc_wrap = alloc;
	do_kfree_wrap = free;
	strncpy(kmalloc_name, name, 128);
	mutex_create(&km_m, 0);
	if(init)
		init(KMALLOC_ADDR_START, KMALLOC_ADDR_END);
}

static addr_t do_kmalloc(size_t sz, char align, char *file, int line)
{
	if(!do_kmalloc_wrap)
		panic(PANIC_MEM | PANIC_NOSYNC, "No kernel-level allocator installed!");
	mutex_acquire(&km_m);
	addr_t ret = do_kmalloc_wrap(sz, align);
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
	if(do_kfree_wrap)
		do_kfree_wrap(pt);
	else
		panic(PANIC_MEM | PANIC_NOSYNC, "No kfree installed!");
	mutex_release(&km_m);
}
