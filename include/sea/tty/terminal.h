#ifndef __SEA_TTY_TERMINAL_H
#define __SEA_TTY_TERMINAL_H

#include <sea/tty/termios.h>
#include <sea/mutex.h>
#include <sea/ll.h>
#define TTY_IBLEN 256

#define KERN_PANIC 8
#define KERN_CRIT 7
#define KERN_ERROR 6
#define KERN_WARN 5
#define KERN_MILE 4
#define KERN_INFO 3
#define KERN_MSG 2
#define KERN_DEBUG 1
#define KERN_EVERY 0

struct vterm {
	char flag;
	volatile int x, ox, y, oy, f, b, w, h, bd, fw, fh, mode, es, scrollt, scrollb;
	char *vmem, *cur_mem, *video;
	char input[TTY_IBLEN];
 	volatile int inpos;
	volatile int reading;
	unsigned char *font;
	int tty;
	char no_wrap;
	mutex_t wlock, inlock;
	struct llist input_block;
	struct termios term;
	struct renderer *rend;
	struct console_driver *driver;
};

struct renderer {
		void (*scroll)(struct vterm *);
		void (*scroll_up)(struct vterm *);
		void (*update_cursor)(struct vterm *);
		void (*clear)(struct vterm *);
		void (*putch)(struct vterm *, char);
		void (*switch_in)(struct vterm *);
		void (*clear_cursor)(struct vterm *);
};

struct console_driver {
		void (*init)(struct vterm *);
		char *name;
};

void console_create(struct vterm *con);
void console_destroy(struct vterm *con);
void console_kernel_puts(char *s);
void console_initialize_vterm(struct vterm *con, struct console_driver *driver);
void console_switch(struct vterm *n);
void console_init_stage1();
void console_init_stage2();

void arch_console_init_stage1();

int tty_read_escape_seq(struct vterm *con, char *seq);

extern struct vterm consoles[];
extern struct vterm *current_console, *kernel_console, *log_console;

void console_puts(struct vterm *c, char *s);
int tty_write(int min, char *buf, size_t len);
int tty_read(int min, char *buf, size_t len);
int tty_close(int min);
int tty_open(int min);
void console_init_stage1();
void console_init_stage2();
int ttyx_rw(int rw, int min, char *buf, size_t count);
int tty_rw(int rw, int m, char *buf, size_t c);
int tty_ioctl(int min, int cmd, long arg);
int ttyx_ioctl(int min, int cmd, long arg);
void tty_init(struct vterm **);
int serial_rw(int, int, char *, size_t);

#endif
