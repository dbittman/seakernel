#include <sea/cpu/interrupt.h>
#include <modules/keymap.h>
#include <modules/keyboard.h>
#include <sea/tm/signal.h>
#include <sea/tm/process.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/interrupt.h>
#include <stdatomic.h>
#include <sea/asm/system.h>
#include <sea/errno.h>
#include <sea/cpu/cpu-io.h>
#include <sea/vsprintf.h>

void flush_port(void)
{
	unsigned temp;
	do
	{
		temp = inb(0x64);
		if((temp & 0x01) != 0)
		{
			(void)inb(0x60);
			continue;
		}
	} while((temp & 0x02) != 0);
}
#include <sea/dm/char.h>
#include <sea/fs/inode.h>

#define BUFFER_SIZE 128
unsigned char buffer[BUFFER_SIZE];
struct spinlock lock;
size_t head = 0;
size_t tail = 0;
size_t count = 0;

int irqk=0;
unsigned char last = 0;
void __int_handle(struct registers *regs, int int_no, int flags)
{
	unsigned char scancode = inb(0x60);
	if(scancode == 0x1d && last == 0xe1)
		asm("int $0x3");
	last = scancode;
	spinlock_acquire(&lock);
	if(count < BUFFER_SIZE) {
		buffer[head++ % BUFFER_SIZE] = scancode;
		count++;
	}
	spinlock_release(&lock);
}

static unsigned char __read_data(void)
{
	unsigned char ret = 0;
	int old = cpu_interrupt_set(0);
	spinlock_acquire(&lock);
	if(count > 0) {
		ret = buffer[tail++ % BUFFER_SIZE];
		count--;
	}
	spinlock_release(&lock);
	cpu_interrupt_set(old);
	return ret;
}

int kb_select(struct file *file, int rw)
{
	int ret = 1;
	if(rw == READ) {
		int old = cpu_interrupt_set(0);
		spinlock_acquire(&lock);
		if(count == 0)
			ret = 0;
		spinlock_release(&lock);
		cpu_interrupt_set(old);
	}
	return ret;
}

ssize_t kb_rw(int rw, struct file *file, off_t off, unsigned char *buf, size_t length)
{
	if(rw == READ) {
		/* TODO: block if we don't have enough data ... */
		for(size_t i=0;i<length;i++) {
			unsigned char val = __read_data();
			if(val == 0)
				return i;
			*buf++ = val;
		}
		return length;
	}
	return -EIO;
}

int keyboard_major;

struct kdevice kbkd = {
	.rw = kb_rw,
	.select = kb_select,
	.create = 0,
	.ioctl = 0,
	.open = 0,
	.close = 0,
	.destroy = 0,
	.name = "keyboard",
};

int module_install(void)
{
	printk(1, "[keyboard]: Driver loading\n");
	spinlock_create(&lock);
	irqk = cpu_interrupt_register_handler(IRQ1, __int_handle);
	flush_port();
	keyboard_major = dm_device_register(&kbkd);
	sys_mknod("/dev/keyboard", S_IFCHR | 0644, GETDEV(keyboard_major, 0));
	printk(1, "[keyboard]: initialized keyboard\n");
	return 0;
}

int module_exit(void)
{
	/* TODO */
	return 0;
}

int module_deps(char *b)
{
	return CONFIG_VERSION_NUMBER;
}

