#ifndef __SEA_DM_BLOCK_H
#define __SEA_DM_BLOCK_H

#define BCACHE_READ 1
#define BCACHE_WRITE 2
#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/fs/inode.h>

typedef struct blockdevice_s {
	int blksz;
	int (*rw)(int mode, int minor, u64 blk, char *buf);
	int (*rw_multiple)(int mode, int minor, u64, char *buf, int);
	int (*ioctl)(int min, int cmd, long arg);
	int (*select)(int min, int rw);
	unsigned char cache;
	mutex_t acl;
} blockdevice_t;

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

void dm_block_cache_init();
int dm_proc_read_bcache(char *buf, int off, int len);
int dm_get_block_cache(int dev, u64 blk, char *buf);
int dm_cache_block(int dev, u64 blk, int sz, char *buf);
int dm_write_block_cache(int dev, u64 blk);
int dm_disconnect_block_cache(int dev);

#endif
