#ifndef DEV_H
#define DEV_H

#include <sea/dm/dev.h>

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
