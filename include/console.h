#ifndef CONSOLE_H
#define CONSOLE_H
#include <termios.h>
#include <mutex.h>
#define TTY_IBLEN 256

#define HOLY_SHITBALLS 9
#define KERN_PANIC 8
#define KERN_CRIT 7
#define KERN_ERROR 6
#define KERN_WARN 5
#define KERN_MILE 4
#define KERN_INFO 3
#define KERN_MSG 2
#define KERN_DEBUG 1
#define KERN_EVERY 0

#define VIDEO_MEMORY 0xb8000

typedef struct vterm_s {
	char flag;
	volatile int x, ox, y, oy, f, b, w, h, bd, fw, fh, mode, es, scrollt, scrollb;
	char *vmem, *cur_mem, *video;
	char input[TTY_IBLEN];
 	volatile int inpos;
	volatile int reading;
	unsigned char *font;
	int tty;
	char nocur, no_wrap;
	mutex_t wlock, inlock;
	unsigned exlock;
	struct termios term;
	struct renderer {
		void (*scroll)(struct vterm_s *);
		void (*scroll_up)(struct vterm_s *);
		void (*update_cursor)(struct vterm_s *);
		void (*clear)(struct vterm_s *);
		void (*putch)(struct vterm_s *, char);
		void (*switch_in)(struct vterm_s *);
		void (*clear_cursor)(struct vterm_s *);
	} rend;
	struct console_driver_s {
		void (*init)(struct vterm_s *);
		char *name;
	} *driver;
} vterm_t;
typedef struct console_driver_s console_driver_t;
extern vterm_t consoles[];

extern vterm_t *curcons, *kernel_console, *log_console;

void set_text_mode();
int tty_write(int min, char *buf, int len);
int tty_read(int min, char *buf, int len);
int tty_close(int min);
int tty_open(int min);
void console_init_stage1();
void console_init_stage2();
void switch_console(vterm_t *new);
void clear_console(int);
int set_console_font(int c, int fw, int fh, int es, unsigned char *fnt);
int ttyx_rw(int rw, int min, char *buf, int count);
int tty_rw(int rw, int m, char *buf, int c);
int serial_rw(int, int, char *, int);
int tty_ioctl(int min, int cmd, int arg);
int ttyx_ioctl(int min, int cmd, int arg);
void puts(char *s);
void tty_init(vterm_t **, vterm_t **);
void create_console(vterm_t *con);
void destroy_console(vterm_t *con);
void init_console(vterm_t *con, console_driver_t *driver);
int read_escape_seq(vterm_t *, char *seq);
#endif
