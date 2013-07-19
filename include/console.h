#ifndef CONSOLE_H
#define CONSOLE_H
#include <termios.h>
#include <mutex.h>
#include <ll.h>
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
	struct renderer {
		void (*scroll)(struct vterm *);
		void (*scroll_up)(struct vterm *);
		void (*update_cursor)(struct vterm *);
		void (*clear)(struct vterm *);
		void (*putch)(struct vterm *, char);
		void (*switch_in)(struct vterm *);
		void (*clear_cursor)(struct vterm *);
	} rend;
	struct console_driver_s {
		void (*init)(struct vterm *);
		char *name;
	} *driver;
};
typedef struct console_driver_s console_driver_t;
extern struct vterm consoles[];

extern struct vterm *curcons, *kernel_console, *log_console;
void console_puts(struct vterm *c, char *s);
void set_text_mode();
int tty_write(int min, char *buf, size_t len);
int tty_read(int min, char *buf, size_t len);
int tty_close(int min);
int tty_open(int min);
void console_init_stage1();
void console_init_stage2();
void switch_console(struct vterm *new);
void clear_console(int);
int set_console_font(int c, int fw, int fh, int es, unsigned char *fnt);
int ttyx_rw(int rw, int min, char *buf, size_t count);
int tty_rw(int rw, int m, char *buf, size_t c);
int serial_rw(int, int, char *, size_t);
int tty_ioctl(int min, int cmd, long arg);
int ttyx_ioctl(int min, int cmd, long arg);
void puts(char *s);
void tty_init(struct vterm **);
void create_console(struct vterm *con);
void destroy_console(struct vterm *con);
void init_console(struct vterm *con, console_driver_t *driver);
int read_escape_seq(struct vterm *, char *seq);
#endif
