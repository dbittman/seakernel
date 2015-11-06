#include <sea/asm/system.h>
#include <sea/mutex.h>
#include <sea/tty/terminal.h>
#include <sea/mm/vmm.h>
extern struct console_driver crtc_drv;
#define VIDEO_MEMORY (0xb8000 + MEMMAP_KERNEL_START)

void arch_console_init_stage1(void)
{
	kernel_console->vmem=kernel_console->cur_mem
						=kernel_console->video=(char *)VIDEO_MEMORY;
	console_initialize_vterm(kernel_console, &crtc_drv);
}
