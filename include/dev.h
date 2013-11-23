#ifndef DEV_H
#define DEV_H

#include <types.h>
#include <mutex.h>

#define OPEN 0
#define CLOSE 1
#define READ 2
#define WRITE 3
#define OTHER 4
#define IOCTL 4

#define NUM_DT 4
#define DT_CHAR 0
#define DT_BLOCK 1

#define ASY_RW 0
#define SIM_RW 1

#define MAJOR(a) (((uint32_t)a)>>16)
#define MINOR(a) (((uint32_t)a)&0xffff)

#define NUM_MINOR 0x10000

#define SETDEV(j, n, c) c = (((uint16_t)j)*NUM_MINOR+((uint16_t)n))

#define GETDEV(maj, min) (((uint16_t)maj)*NUM_MINOR+((uint16_t)min))

#define DH_SZ 64
typedef struct device_ss {
	void *ptr;
	int beta;
	struct device_ss *next;
} device_t;

struct devhash_s {
	device_t *devs[DH_SZ];
	mutex_t lock;
};
	
void init_block_devs();
void send_sync_char();
void send_sync_block();
int add_device(int type, int major, void *str);
int remove_device(int type, int major);
device_t *get_n_device(int type, int n);
device_t *get_device(int type, int major);
int blockdev_select(struct inode *in, int rw);
int chardev_select(struct inode *in, int rw);
int pipedev_select(struct inode *in, int rw);
int dm_ioctl(int type, dev_t dev, int cmd, long arg);

#endif
