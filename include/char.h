#ifndef CHAR_H
#define CHAR_H
#include <kernel.h>
#include <dev.h>
typedef struct chardevice_s {
	int (*func)(int mode, int minor, char *buf, size_t count);
	int (*ioctl)(int min, int cmd, long arg);
	int (*select)(int min, int rw);
} chardevice_t;
void init_char_devs();
chardevice_t *set_chardevice(int maj, int (*f)(int, int, char*, size_t), 
	int (*c)(int, int, long), int (*s)(int, int));
int char_rw(int rw, dev_t dev, char *buf, size_t len);
int char_ioctl(dev_t dev, int cmd, int arg);
int set_availablecd(int (*f)(int, int, char*, size_t), int (*c)(int, int, long), 
	int (*s)(int, int));
void unregister_char_device(int n);
int ttyx_rw(int rw, int min, char *buf, size_t count);
int tty_rw(int rw, int min, char *buf, size_t count);
int tty_select(int, int);
int ttyx_select(int, int);
#endif
