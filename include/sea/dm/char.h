#ifndef __SEA_DM_CHAR_H
#define __SEA_DM_CHAR_H

#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
struct chardevice {
	ssize_t (*func)(int direction, struct file *f, off_t off, char *buf, size_t count);
	int (*ioctl)(struct file *, int cmd, long arg);
	int (*select)(struct file *, int rw);
};

struct chardevice *dm_set_char_device(int maj,
		ssize_t (*rw)(int, struct file *, off_t off, char*, size_t),
		int (*c)(struct file *, int, long),
		int (*s)(struct file *, int));

int dm_set_available_char_device( 
		ssize_t (*rw)(int, struct file *, off_t off, char*, size_t),
		int (*c)(struct file *, int, long),
		int (*s)(struct file *, int));
int dm_char_rw(int rw, struct file *file, off_t off, char *buf, size_t len);
int dm_char_ioctl(struct file *file, int cmd, long arg);
int dm_chardev_select(struct file *f, int rw);

void dm_init_char_devices();
void dm_unregister_char_device(int n);
void dm_send_sync_char();
int ttyx_rw(int rw, int min, char *buf, size_t count);
int tty_rw(int rw, int min, char *buf, size_t count);
int tty_select(int, int);
int ttyx_select(int, int);
int net_char_select(int, int);
int net_char_rw(int rw, int min, char *buf, size_t count);
int net_char_ioctl(dev_t dev, int cmd, long arg);
#endif
