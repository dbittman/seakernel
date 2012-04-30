#include <kernel.h>
#include <memory.h>
#include "bga.h"
#include <task.h>

int modes(int m, int *x, int *y, int *bd)
{
	switch(m)
	{
		case 271:
			*x = 320;
			*y = 200;
			*bd=24;
			break;
		case 274:
			*x = 640;
			*y = 480;
			*bd=24;
			break;
		case 277:
			*x = 800;
			*y = 600;
			*bd=24;
			break;
		case 280:
			*x = 1024;
			*y = 768;
			*bd=24;
			break;
		case 283:
			*x = 1280;
			*y = 1024;
			*bd=24;
			break;
		default:
			kprintf("Mode %d is not supported by this driver at this time\n");
			return 1;
	}
	return 0;
}

void bga_write(unsigned short IndexValue, unsigned short DataValue)
{
    outw(VBE_DISPI_IOPORT_INDEX, IndexValue);
    outw(VBE_DISPI_IOPORT_DATA, DataValue);
}
 
unsigned short bga_read(unsigned short IndexValue)
{
    outw(VBE_DISPI_IOPORT_INDEX, IndexValue);
    return inw(VBE_DISPI_IOPORT_DATA);
}
 
int is_bochs_video(void)
{
    return (bga_read(VBE_DISPI_INDEX_ID) == VBE_DISPI_ID4);
}
 
void ba_set_vm(unsigned int W, unsigned int H, unsigned int BD, int UseLinearFrameBuffer, int ClearVideoMemory)
{
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, W);
    bga_write(VBE_DISPI_INDEX_YRES, H);
    bga_write(VBE_DISPI_INDEX_BPP, BD);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED |
    (UseLinearFrameBuffer ? VBE_DISPI_LFB_ENABLED : 0) |
        (ClearVideoMemory ? 0 : VBE_DISPI_NOCLEARMEM));
}

int pre=0;

int switch_mode(int c, int cmd, int m)
{
	int i = 0;
	for(i=0;i<=(1280 * 1024 * 3);i+=0x1000)
	{
		vm_map_all(0xE0000000+i, VBE_DISPI_LFB_PHYSICAL_ADDRESS+i, PAGE_PRESENT | PAGE_WRITE);
	}
	int x, y, bd;
	int r = modes(m, &x, &y, &bd);
	if(r == 1) {
		if(c == curcons)
			consoles[c].video = consoles[c].cur_mem = (char *)VIDEO_MEMORY;
		return -1;
	}
	ba_set_vm(x, y, 24, 1, 1);
	setup_console_video(c, m, (char *)0xE0000000, x, y, 3);
	set_console_font(c, 8, 16, 0, 0);
	
	if(pre != m)
	{
		pre=m;
		setup_console_video(9, m, (char *)0xE0000000, x, y, 3);
		set_console_font(9, 8, 16, 0, 0);
		//setup_console_video(0, m, (char *)0xE0000000, x, y, 3);
		//set_console_font(0, 8, 16, 0, 0);
	}
	
	return 0;
}

int module_install()
{
	if(!is_bochs_video()) {
		printk(1, "[bga]: Not a bochs video controller!\n");
		return -1;
	}
	memcpy(consoles[current_task->tty].vmem, consoles[current_task->tty].cur_mem, consoles[current_task->tty].w * consoles[current_task->tty].h * consoles[current_task->tty].bd);
	if(sys_ioctl(0, 128, (unsigned)switch_mode) != 128)
	{
		kprintf("Could not allocate a tty callback for mode switch!\n");
		return -1;
	}
	return 0;
}

int module_exit()
{
	return 0;
	int i;
	for(i=0;i<(1280 * 1024 * 3);i+=0x1000)
	{
		vm_unmap_only(0xE0000000+i);
	}
	return 0;
}
int module_deps(char *b)
{
	return KVERSION;
}
