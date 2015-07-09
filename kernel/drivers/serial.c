#include <sea/mutex.h>
#include <sea/errno.h>
#include <sea/dm/dev.h>
#include <sea/serial.h>
#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/loader/symbol.h>

static mutex_t serial_m;
static char serial_initialized=0;

#if ! CONFIG_SERIAL_DEBUG
#define DISABLE_SERIAL 1
#endif

static int serial_enable = 0;
static int serial_debug_port_minor = 0;

void serial_console_puts_nolock(int minor, char *s)
{
	if(!serial_initialized || !serial_enable)
		return;
	while(*s)
	{
		arch_serial_write(serial_debug_port_minor, *s);
		s++;
	}
}

void serial_console_puts(int minor, char *s)
{
	mutex_acquire(&serial_m);
	serial_console_puts_nolock(minor, s);
	mutex_release(&serial_m);
}

int serial_rw(int rw, int min, char *b, size_t c)
{
	if(!serial_initialized || !serial_enable) 
		return -EINVAL;
	int i=c;
	if(rw == WRITE)
	{
		mutex_acquire(&serial_m);
		while(i)
		{
			arch_serial_write(serial_debug_port_minor, *(b++));
			i--;
		}
		mutex_release(&serial_m);
		return c;
	} else if(rw == READ)
	{
		while(i--) {
			while(arch_serial_received(serial_debug_port_minor) == 0)
			{
				if(tm_thread_got_signal(current_thread))
					return -EINTR;
			}
			*(b) = arch_serial_read(serial_debug_port_minor);
			b++;
		}
		return c;
	}
	return 0;
}

void serial_init()
{
#if ! DISABLE_SERIAL
	/* TODO: make this not schedule */
	mutex_create(&serial_m, 0);
	arch_serial_init(&serial_debug_port_minor, &serial_enable);

	serial_initialized = 1;
	serial_console_puts_nolock(0, "[kernel]: started debug serial output\n");
#endif
#if CONFIG_MODULES
	loader_add_kernel_symbol(serial_console_puts);
	loader_add_kernel_symbol(serial_console_puts_nolock);
#endif
}

void serial_disable(void)
{
	serial_enable = 0;
}

