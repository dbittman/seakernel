#ifndef __SEA_DM_CHAR_H
#define __SEA_DM_CHAR_H

#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
struct chardevice {
	int (*func)(int mode, int minor, char *buf, size_t count);
	int (*ioctl)(int min, int cmd, long arg);
	int (*select)(int min, int rw);
};

struct chardevice *dm_set_char_device(int maj, int (*f)(int, int, char*, size_t), 
	int (*c)(int, int, long), int (*s)(int, int));

int dm_set_available_char_device(int (*f)(int, int, char*, size_t), 
	int (*c)(int, int, long), int (*s)(int, int));

void dm_init_char_devices();
int dm_char_rw(int rw, dev_t dev, char *buf, size_t len);
void dm_unregister_char_device(int n);
int dm_char_ioctl(dev_t dev, int cmd, long arg);
int dm_chardev_select(struct inode *in, int rw);
void dm_send_sync_char();
int ttyx_rw(int rw, int min, char *buf, size_t count);
int tty_rw(int rw, int min, char *buf, size_t count);
int tty_select(int, int);
int ttyx_select(int, int);
int net_char_select(int, int);
int net_char_rw(int rw, int min, char *buf, size_t count);
int net_char_ioctl(dev_t dev, int cmd, long arg);
#endif
