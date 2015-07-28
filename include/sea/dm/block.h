#ifndef __SEA_DM_BLOCK_H
#define __SEA_DM_BLOCK_H

#define BCACHE_READ 1
#define BCACHE_WRITE 2
#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/tm/kthread.h>
#include <sea/lib/queue.h>
#include <sea/ll.h>
#include <sea/lib/hash.h>

#define BLOCK_CACHE_OVERWRITE 1

typedef struct blockdevice_s {
	int blksz;
	int (*rw)(int mode, int minor, u64 blk, char *buf);
	int (*rw_multiple)(int mode, int minor, u64, char *buf, int);
	int (*ioctl)(int min, int cmd, long arg);
	int (*select)(int min, int rw);
	mutex_t acl;
	struct queue wq;
	struct kthread elevator;
	struct hash_table cache;
	mutex_t cachelock;
} blockdevice_t;

struct ioreq {
	uint64_t block;
	size_t count;
	int refs;
	int direction;
	int flags;
	blockdevice_t *bd;
	dev_t dev;
	struct queue_item qi;
	struct llist blocklist;
};

#define IOREQ_COMPLETE 1
#define IOREQ_FAILED   2

struct buffer {
	blockdevice_t *bd;
	int refs;
	int flags;
	uint64_t block;
	dev_t dev; //TODO: please, please, fix this crap
	struct llistnode lnode, dlistnode;
	struct queue_item qi;
	char data[];
};

#define BUFFER_DIRTY 1
#define BUFFER_DLIST 2
#define BUFFER_WRITEPENDING 4

blockdevice_t *dm_set_block_device(int maj, int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int));

int dm_set_available_block_device(int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int));

void dm_unregister_block_device(int n);
void dm_init_block_devices();
int dm_block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd);
int dm_do_block_rw_multiple(int rw, dev_t dev, u64 blk, char *buf, int count, blockdevice_t *bd);
int dm_do_block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd);
int dm_block_read(dev_t dev, off_t posit, char *buf, size_t c);
int dm_block_write(dev_t dev, off_t posit, char *buf, size_t count);
int dm_block_device_rw(int mode, dev_t dev, off_t off, char *buf, size_t len);
int dm_block_ioctl(dev_t dev, int cmd, long arg);
int dm_block_device_select(dev_t dev, int rw);
int dm_blockdev_select(struct inode *in, int rw);
void dm_send_sync_block();
void block_cache_init(void);
void block_buffer_init(void);
int buffer_sync_all_dirty(void);
int block_elevator_main(struct kthread *kt, void *arg);
void block_elevator_add_request(struct ioreq *req);

struct buffer *dm_block_cache_get(blockdevice_t *bd, uint64_t block);
int dm_block_cache_insert(blockdevice_t *bd, uint64_t block, struct buffer *, int flags);

int block_cache_request(struct ioreq *req, off_t initial_offset, size_t total_bytecount, char *buffer);

struct buffer *buffer_create(blockdevice_t *bd, dev_t dev, uint64_t block, int flags, char *data);
void buffer_put(struct buffer *buf);
void buffer_inc_refcount(struct buffer *buf);
int block_cache_get_bufferlist(struct llist *blist, struct ioreq *req);
struct ioreq *ioreq_create(blockdevice_t *bd, dev_t dev, int, uint64_t start, size_t count);
void ioreq_put(struct ioreq *req);
#endif
