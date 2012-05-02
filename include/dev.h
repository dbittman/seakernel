#ifndef DEV_H
#define DEV_H

#include <kernel.h>
#include <task.h>
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

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define SETDEV(j, n, c) \
		c = j*256; \
		c += n;

#define GETDEV(maj, min) (maj*256+min)

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
		
		
int char_rw(int rw, int dev, char *buf, int len);
int block_device_rw(int mode, int dev, int off, char *buf, int len);
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

#endif
