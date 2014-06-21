/* console.c: Copyright (c) 2010 Daniel Bittman
 * Provides functions for the virtual aspect of terminals. 
 */
#include <sea/kernel.h>
#include <sea/asm/system.h>
#include <sea/mutex.h>
#include <sea/tty/terminal.h>

struct vterm *current_console=0;
struct vterm *kernel_console, *log_console=0;

/* Simple way to display messages before tty and vsprintf get working */
void console_puts(struct vterm *c, char *s)
{
	while(s && *s && c) {
		c->rend->putch(c, *s);
		if(*(s++) == '\n')
			c->rend->putch(c, '\r');
	}
}

void console_kernel_puts(char *s)
{
	console_puts(kernel_console, s);
}

void console_destroy(struct vterm *con)
{
	if(con == current_console)
		current_console = kernel_console;
	kfree(con->vmem);
	mutex_destroy(&con->wlock);
	mutex_destroy(&con->inlock);
	con->flag=0;
}

void console_create(struct vterm *con)
{
	if(con->flag) return;
	con->term.c_lflag=ECHO | ISIG | ECHONL | ICANON;
	con->term.c_oflag=OPOST | ONLCR;
	con->term.c_iflag=ICRNL;
	mutex_create(&con->wlock, 0);
	mutex_create(&con->inlock, 0);
	ll_create(&con->input_block);
	con->flag=1;
}

void console_initialize_vterm(struct vterm *con, struct console_driver *driver)
{
	driver->init(con);
	con->driver = driver;
	printk(0, "[console]: Initialized console %d (%x:%x): %s\n", con->tty, 
				con, driver->init, driver->name);
}

void console_switch(struct vterm *n)
{
	/* Copy screen to old console */
	struct vterm *old = current_console;
	mutex_acquire(&old->wlock);
	memcpy(current_console->vmem, (char *)current_console->video, 
				current_console->h*current_console->w*current_console->bd);
	current_console->cur_mem = current_console->vmem;
	current_console = n;
	current_console->cur_mem = (char *)current_console->video;
	if(current_console->rend->switch_in)
		current_console->rend->switch_in(current_console);
	memcpy(current_console->cur_mem, current_console->vmem, current_console->w*current_console->h*current_console->bd);
	if(current_console->rend->update_cursor)
		current_console->rend->update_cursor(current_console);
	mutex_release(&old->wlock);
}

void console_init_stage1()
{
	tty_init(&kernel_console);
	console_create(kernel_console);
	arch_console_init_stage1();
	current_console = kernel_console;
	current_console->rend->clear(current_console);
	printk(0, "[console]: Video output ready\n");
}

