#ifndef SEA_SWAP_H
#define SEA_SWAP_H
#include <sea/mm/vmm.h>
#include <sea/tm/kthread.h>
#define SW_FORCE 1
#define SW_EMPTY 2
#define SW_ENABLE 4

typedef struct {
	unsigned page;
	unsigned slot;
	unsigned pid;
} page_index_t;

/* A swap device refers to any device that can be accessed like a block device */
typedef struct swapdevice_s
{
	int dev;
	unsigned int size;
	unsigned int flags;
	unsigned char old_cache;
	unsigned bytes_used;
	char node[16];
	unsigned blocksize;
	page_index_t *page_index;
	unsigned char *block_index;
	unsigned pi_size, nslots;
	mutex_t *lock;
	unsigned uslots;
	struct swapdevice_s *next, *prev;
} swapdev_t;

extern swapdev_t *swaplist;
extern volatile unsigned num_swapdev;

int __KT_pager(struct kthread *kt, void *);

static inline char valid_swappable(addr_t x)
{
	if(x >= (addr_t)(1024*0x1000*id_tables) && x < TOP_TASK_MEM)
		return 1;
	else
		return 0;
}

#endif
