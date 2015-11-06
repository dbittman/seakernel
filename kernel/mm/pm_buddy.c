#include <sea/mm/vmm.h>
#include <stdbool.h>
#include <sea/kernel.h>
#include <sea/lib/bitmap.h>
#include <sea/mutex.h>
#include <sea/fs/kerfs.h>
#include <sea/lib/stack.h>
#define IS_POWER2(x) ((x != 0) && ((x & (~x + 1)) == x))

#define MIN_PHYS_MEM 0

#define MAX_ORDER 20
#define MIN_SIZE PAGE_SIZE
#define MAX_SIZE ((addr_t)MIN_SIZE << MAX_ORDER)
#define MEMORY_SIZE (MAX_SIZE - MIN_PHYS_MEM)

#define NOT_FREE (-1)
struct mutex pm_buddy_mutex;
uint8_t *bitmaps[MAX_ORDER + 1];

struct stack freelists[MAX_ORDER+1];
static char static_bitmaps[((MEMORY_SIZE / MIN_SIZE) / 8) * 2];
static bool inited = false;
static size_t num_allocated[MAX_ORDER + 1];

static _Atomic size_t free_memory = 0;

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

static inline size_t buddy_order_max_blocks(int order)
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

	if(stack_is_empty(&freelists[order])) {
		addr_t a = __do_pmm_buddy_allocate(length * 2);
		int bit = a / length;

		struct stack_elem *elem1 = (void *)(a + PHYS_PAGE_MAP);
		struct stack_elem *elem2 = (void *)(a + length + PHYS_PAGE_MAP);

		stack_push(&freelists[order], elem1, (void *)a);
		stack_push(&freelists[order], elem2, (void *)(a + length));
	}

	addr_t address = (addr_t)stack_pop(&freelists[order]);
	int bit = address / length;
	assert(!bitmap_test(bitmaps[order], bit));
	bitmap_set(bitmaps[order], bit);
	num_allocated[order]++;


	return address;
}

static int deallocate(addr_t address, int order)
{
	assert(inited);
	int bit = address / ((addr_t)MIN_SIZE << order);
	if(!bitmap_test(bitmaps[order], bit)) {
		return deallocate(address, order + 1);
	} else {
		addr_t buddy = address ^ ((addr_t)MIN_SIZE << order);
		int buddy_bit = buddy / ((addr_t)MIN_SIZE << order);
		bitmap_reset(bitmaps[order], bit);

		if(!bitmap_test(bitmaps[order], buddy_bit)) {
			struct stack_elem *elem = (void *)(buddy + PHYS_PAGE_MAP);
			stack_delete(&freelists[order], elem);
			deallocate(buddy > address ? address : buddy, order + 1);
		} else {
			struct stack_elem *elem = (void *)(address + PHYS_PAGE_MAP);
			stack_push(&freelists[order], elem, (void *)address);
		}
		num_allocated[order]--;
		return order;
	}
}

static inline addr_t pmm_buddy_allocate(size_t length)
{
	mutex_acquire(&pm_buddy_mutex);
	addr_t ret = __do_pmm_buddy_allocate(length);
	free_memory -= length;
	mutex_release(&pm_buddy_mutex);
	return ret;
}

static inline void pmm_buddy_deallocate(addr_t address)
{
	mutex_acquire(&pm_buddy_mutex);
	int order = deallocate(address, 0);
	free_memory += MIN_SIZE << order;
	mutex_release(&pm_buddy_mutex);
}

void pmm_buddy_init()
{
	int total = ((MEMORY_SIZE / MIN_SIZE) / (8 * 1024)) * 2 - 1;
	mutex_create(&pm_buddy_mutex, 0);
	addr_t start = (addr_t)static_bitmaps;
	int length = ((MEMORY_SIZE / MIN_SIZE) / (8));
	for(int i=0;i<=MAX_ORDER;i++) {
		bitmaps[i] = (uint8_t *)start;
		memset(bitmaps[i], ~0, length);
		stack_create(&freelists[i], STACK_LOCKLESS);
		start += length;
		length /= 2;
		num_allocated[i] = buddy_order_max_blocks(i);
	}
	inited = true;
}

int kerfs_pmm_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"Free memory: %dKB (%dMB)\n",
			free_memory / 1024, free_memory / (1024 * 1024));
	/*for(int i=0;i<=MAX_ORDER;i++) {
		KERFS_PRINTF(offset, length, buf, current,
			"Order %d: %d / %d\n", i,
			num_allocated[i], buddy_order_max_blocks(i));
	}
	*/
	return current;
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


