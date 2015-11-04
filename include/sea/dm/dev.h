#ifndef __SEA_DM_DEV_H
#define __SEA_DM_DEV_H

#include <sea/types.h>
#include <sea/mutex.h>

#define OPEN 0
#define CLOSE 1
#define READ 2
#define WRITE 3
#define OTHER 4
#define IOCTL 4

#define NUM_DT 4
#define DT_CHAR   0
#define DT_BLOCK  1

#define ASY_RW 0
#define SIM_RW 1

#define MAJOR(a) (((uint32_t)a)>>16)
#define MINOR(a) (((uint32_t)a)&0xffff)

#define NUM_MINOR 0x10000

#define SETDEV(j, n, c) c = (((uint16_t)j)*NUM_MINOR+((uint16_t)n))

#define GETDEV(maj, min) (((uint16_t)maj)*NUM_MINOR+((uint16_t)min))

#define DH_SZ 64
struct device {
	void *ptr;
	int beta;
	struct device *next;
};

struct devhash_s {
	struct device *devs[DH_SZ];
	struct mutex lock;
};
	
void dm_init();
void dm_sync();
int dm_ioctl(int type, dev_t dev, int cmd, long arg);
struct device *dm_get_device(int type, int major);
struct device *dm_get_enumerated_device(int type, int n);
int dm_add_device(int type, int major, void *str);
struct device *dm_get_enumerated_device(int type, int n);
int dm_remove_device(int type, int major);

#endif
