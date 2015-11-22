#ifndef __SEA_DM_BLOCK_H
#define __SEA_DM_BLOCK_H

#define BCACHE_READ 1
#define BCACHE_WRITE 2
#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/tm/kthread.h>
#include <sea/lib/queue.h>
#include <sea/lib/linkedlist.h>
#include <sea/lib/hash.h>
#include <sea/tm/blocking.h>
#define BLOCK_CACHE_OVERWRITE 1

struct ioreq {
	uint64_t block;
	size_t count;
	int refs;
	int direction;
	int flags;
	struct blockdev *bd;
	dev_t dev;
	struct blocklist blocklist;
};

#define IOREQ_COMPLETE 1
#define IOREQ_FAILED   2

struct buffer {
	struct blockdev *bd;
	int refs;
	int flags;
	uint64_t __block, trueblock;
	dev_t dev; //TODO: please, please, fix this crap
	struct linkedentry lnode;
	struct linkedentry dlistnode;
	struct queue_item qi;
	struct hashelem hash_elem;
	char data[];
};

#define buffer_block(b) (b->__block + b->bd->partbegin)

#define BUFFER_DIRTY 1
#define BUFFER_DLIST 2
#define BUFFER_WRITEPENDING 4
#define BUFFER_LOCKED       8

void block_cache_init(void);
void block_buffer_init(void);
int buffer_sync_all_dirty(void);
int block_elevator_main(struct kthread *kt, void *arg);
bool block_elevator_add_request(void *req);

struct buffer *dm_block_cache_get(struct blockdev *bd, uint64_t block);
int dm_block_cache_insert(struct blockdev *bd, uint64_t block, struct buffer *, int flags);

int block_cache_request(struct ioreq *req, off_t initial_offset, size_t total_bytecount, unsigned char *buffer);

struct buffer *buffer_create(struct blockdev *bd, dev_t dev, uint64_t block, int flags, unsigned char *data);
void buffer_put(struct buffer *buf);
void buffer_inc_refcount(struct buffer *buf);
struct buffer *block_cache_get_first_buffer(struct ioreq *req);
int block_cache_get_bufferlist(struct linkedlist *blist, struct ioreq *req);
struct ioreq *ioreq_create(struct blockdev *bd, dev_t dev, int, uint64_t start, size_t count);
void ioreq_put(struct ioreq *req);
#endif

