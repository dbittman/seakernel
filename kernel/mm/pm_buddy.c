#include <sea/mm/vmm.h>
#include <stdbool.h>
#include <sea/kernel.h>
#include <sea/lib/bitmap.h>
#include <sea/mutex.h>
#include <sea/ll.h>
#include <sea/fs/kerfs.h>
#define IS_POWER2(x) ((x != 0) && ((x & (~x + 1)) == x))

#define MAX_PHYS_MEM 0x1000000000
#define MIN_PHYS_MEM 0

#define MEMORY_SIZE (MAX_PHYS_MEM - MIN_PHYS_MEM)
#define MAX_ORDER 21
#define MIN_SIZE 0x1000
#define MAX_SIZE ((addr_t)MIN_SIZE << MAX_ORDER)

#define NOT_FREE (-1)
mutex_t pm_buddy_mutex;
uint8_t *bitmaps[MAX_ORDER + 1];

struct llist freelists[MAX_ORDER+1];
static char static_bitmaps[((MEMORY_SIZE / MIN_SIZE) / 8) * 2];
static bool inited = false;
static size_t num_allocated[MAX_ORDER + 1];

static inline int min_possible_order(addr_t address)
{
	address /= MIN_SIZE;
	int o = 0;
	while(address && !(address & 1)) {
		o++;
		address >>= 1;
	}
	return o;
}

static size_t buddy_order_max_blocks(int order)
{
	return MEMORY_SIZE / ((addr_t)MIN_SIZE << order);
}

/* TODO: - OOM
 *       - Limit memory locations
 */

static addr_t __do_pmm_buddy_allocate(size_t length)
{
	assert(inited);
	if(!IS_POWER2(length))
		panic(0, "can only allocate in powers of 2");
	if(length < MIN_SIZE)
		panic(0, "length less than minimum size");
	if(length > MAX_SIZE) {
		panic(PANIC_NOSYNC, "out of physical memory");
	}

	int order = min_possible_order(length);

	if(ll_is_empty(&freelists[order])) {
		addr_t a = __do_pmm_buddy_allocate(length * 2);
		int bit = a / length;
		ll_do_insert(&freelists[order], (void *)(a + PHYS_PAGE_MAP), (void *)a);
		ll_do_insert(&freelists[order], 
				(void *)(a + length + PHYS_PAGE_MAP), (void *)(a + length));
	}

	struct llistnode *node = freelists[order].head;
	ll_do_remove(&freelists[order], node, 0);
	addr_t address = ll_entry(addr_t, node);
	int bit = address / length;
	assert(!bitmap_test(bitmaps[order], bit));
	bitmap_set(bitmaps[order], bit);
	num_allocated[order]++;

	return address;
}

static void deallocate(addr_t address, int order)
{
	assert(inited);
	int bit = address / ((addr_t)MIN_SIZE << order);
	if(!bitmap_test(bitmaps[order], bit)) {
		deallocate(address, order + 1);
	} else {
		addr_t buddy = address ^ ((addr_t)MIN_SIZE << order);
		int buddy_bit = buddy / ((addr_t)MIN_SIZE << order);
		bitmap_reset(bitmaps[order], bit);
		if(!bitmap_test(bitmaps[order], buddy_bit)) {
			struct llistnode *bn = (void *)(buddy + PHYS_PAGE_MAP);
			ll_do_remove(&freelists[order], bn, 0);
			deallocate(buddy > address ? address : buddy, order + 1);
		} else {
			struct llistnode *node = (void *)(address + PHYS_PAGE_MAP);
			ll_do_insert(&freelists[order], node, (void *)address);
		}
		num_allocated[order]--;
	}
}

addr_t pmm_buddy_allocate(size_t length)
{
	mutex_acquire(&pm_buddy_mutex);
	addr_t ret = __do_pmm_buddy_allocate(length);
	mutex_release(&pm_buddy_mutex);
	return ret;
}

void pmm_buddy_deallocate(addr_t address)
{
	mutex_acquire(&pm_buddy_mutex);
	deallocate(address, 0);
	mutex_release(&pm_buddy_mutex);
}

void pmm_buddy_init()
{
	int total = ((MEMORY_SIZE / MIN_SIZE) / (8 * 1024)) * 2 - 1;
	mutex_create(&pm_buddy_mutex, MT_NOSCHED);
	addr_t start = (addr_t)static_bitmaps;
	int length = ((MEMORY_SIZE / MIN_SIZE) / (8));
	for(int i=0;i<=MAX_ORDER;i++) {
		bitmaps[i] = (uint8_t *)start;
		memset(bitmaps[i], ~0, length);
		ll_create_lockless(&freelists[i]);
		start += length;
		length /= 2;
		num_allocated[i] = buddy_order_max_blocks(i);
	}
	inited = true;
}

int kerfs_pmm_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	for(int i=0;i<=MAX_ORDER;i++) {
		KERFS_PRINTF(offset, length, buf, current,
			"Order %d: %d / %d\n", i,
			num_allocated[i], buddy_order_max_blocks(i));
	}
	return current;
}

