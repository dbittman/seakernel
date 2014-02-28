#ifndef CONSOLE_H
#define CONSOLE_H

#include <sea/tty/terminal.h>

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
void switch_console(struct vterm *);
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
void init_console(struct vterm *con, struct console_driver *driver);
int read_escape_seq(struct vterm *, char *seq);
#endif
