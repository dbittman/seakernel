#ifndef BLOCK_H
#define BLOCK_H

#define BCACHE_READ 1
#define BCACHE_WRITE 2
#include <mutex.h>

typedef struct blockdevice_s {
	int blksz;
	int (*rw)(int mode, int minor, u64 blk, char *buf);
	int (*rw_multiple)(int mode, int minor, u64, char *buf, int);
	int (*ioctl)(int min, int cmd, long arg);
	int (*select)(int min, int rw);
	unsigned char cache;
	mutex_t acl;
} blockdevice_t;

void init_block_devs();
blockdevice_t *set_blockdevice(int maj, int (*f)(int, int, u64, char*), 
	int bs, int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int));

int block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd);

int block_device_rw(int mode, dev_t dev, off_t off, char *buf, size_t len);

int block_ioctl(dev_t dev, int cmd, long arg);

int do_block_rw_multiple(int rw, dev_t dev, u64 blk, char *buf, int count, blockdevice_t *bd);

int do_block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd);

int set_availablebd(int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int));

int block_device_select(dev_t dev, int rw);

void block_cache_init();

int mem_rw(int rw, int min, u64 blk, char *b);

int get_block_cache(dev_t dev, u64 blk, char *buf);

int cache_block(dev_t dev, u64 blk, int sz, char *buf);

int write_block_cache(dev_t dev, u64 blk);

int sync_block_device(dev_t dev);

int disconnect_block_cache(dev_t dev);

int disconnect_block_cache_1(dev_t dev);

int disconnect_block_cache_2(dev_t dev);

int disconnect_block_cache_slow(dev_t dev);

void unregister_block_device(int n);

int block_read(dev_t dev, off_t posit, char *buf, size_t c);

int block_write(dev_t dev, off_t posit, char *buf, size_t count);

#endif
