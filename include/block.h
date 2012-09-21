#ifndef BLOCK_H
#define BLOCK_H

#define BCACHE_READ 1
#define BCACHE_WRITE 2

typedef struct blockdevice_s {
	int blksz;
	int (*rw)(int mode, int minor, u64 blk, char *buf);
	int (*rw_multiple)(int mode, int minor, u64, char *buf, int);
	int (*ioctl)(int min, int cmd, int arg);
	int (*select)(int min, int rw);
	unsigned char cache;
	mutex_t acl;
} blockdevice_t;
void init_block_devs();
blockdevice_t *set_blockdevice(int maj, int (*f)(int, int, u64, char*), 
	int bs, int (*c)(int, int, int), int (*m)(int, int, u64, char *, int), int (*s)(int, int));
int block_rw(int rw, int dev, u64 blk, char *buf, blockdevice_t *bd);


int block_device_rw(int mode, int dev, int off, char *buf, int len);


int block_ioctl(int dev, int cmd, int arg);
int do_block_rw(int rw, int dev, u64 blk, char *buf, blockdevice_t *bd);
int set_availablebd(int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, int), int (*m)(int, int, u64, char *, int), int (*s)(int, int));
int do_block_rw_multiple(int rw, int dev, u64 blk, char *buf, 
	blockdevice_t *bd, int count);
void block_cache_init();
int mem_rw(int rw, int min, u64 blk, char *b);
int get_block_cache(int dev, u64 blk, char *buf);
int cache_block(int dev, u64 blk, int sz, char *buf);
int write_block_cache(int dev, u64 blk);
int sync_block_device(int dev);
int disconnect_block_cache(int dev);
int disconnect_block_cache_1(int dev);
int disconnect_block_cache_2(int dev);
int disconnect_block_cache_slow(int dev);
void unregister_block_device(int n);
#endif
