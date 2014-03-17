/* Simple driver for the CRT controller */
#include <kernel.h>
#include <console.h>
#include <asm/system.h>
#include <task.h>
#include <sea/tty/terminal.h>
#define VIDEO_MEMORY 0xb8000

static char crtc_first_init=0;
void crtc_init_console(struct vterm *con);
void crtc_putchar(char *mem, char c, char attr, int x, int y, int w);
struct console_driver crtc_drv = {
	crtc_init_console,
	"crtc"
};
#define WHITE_ON_BLACK ((0 << 4) | (15 & 0x0F))

void crtc_scrolldown(struct vterm *con)
{
	if(!con || con == kernel_console)
		con=curcons;
	/* Get a space character with the default colour attributes. */
	unsigned short blank = 0x20 /* space */ | (((con->b << 4) | (con->f & 0x0F)) << 8);
	unsigned short *video_memory = (unsigned short *)con->cur_mem + con->w*con->scrollt;
	memmove((char *)video_memory, ((char *)video_memory + con->w*2), 
			((con->scrollb)-1)*con->w*2);
	int i = (con->scrollb-1)*con->w;
	for (i = (con->scrollb-1)*con->w; i < con->scrollb*con->w; i++)
		video_memory[i] = blank;
	con->y = con->scrollb-1;
}

void crtc_update_cursor(struct vterm *con)
{
	if(!con || con == kernel_console)
		con = curcons;
	if(con != curcons)
		return;
	volatile unsigned short position = (con->y * con->w + con->x);
	outb(0x3D4, 0x0F);
	outb(0x3D5, (unsigned char)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (unsigned char )((position>>8)&0xFF));
}

void crtc_console_put(struct vterm *con, char c)
{
	if(!con || con == kernel_console)
		con=curcons;
	if(c == 0x08) {
		if(con->x)
			con->x--;
	}
	else if(c == 0x09)
		con->x = (con->x+8) & ~(8-1);
	else if(c == '\r')
		con->x=0;
	else if(c == '\n')
		con->y++;
	else if(c >= ' ')
	{
		crtc_putchar((char *)con->cur_mem, c, ((con->b << 4) | (con->f & 0x0F))
				, con->x, con->y, con->w);
		con->x++;
	}
	if(!con->no_wrap) {
		if(con->x >= con->w) {
			con->x=0;
			con->y++;
		}
	}
	if(con->y >= con->scrollb && con->rend->scroll)
		crtc_scrolldown(con);
}

void crtc_putchar(char *mem, char c, char attr, int x, int y, int w)
{
	int offset = y * w + x;
	if(april_fools) {
		*(char *)((char *)mem + offset*2+1) = c;
		*(char *)((char *)mem + offset*2) = attr;
	} else {
		*(unsigned char *)((unsigned char *)mem + offset*2) = c;
		*(unsigned char *)((unsigned char *)mem + offset*2 + 1) = attr;
	}
}

void crtc_clear(struct vterm *con)
{
	memset(con->cur_mem, 0, con->w*con->h*2);
	con->x=0;
	con->y=0;
	int x, y;
	for(y=0;y<con->h;y++) {
		for(x=0;x<con->w;x++)
			crtc_putchar(con->cur_mem, ' ', WHITE_ON_BLACK, x, y, con->w);
	}
}

void crtc_clear_cursor(struct vterm *c)
{
	if(!c || c == kernel_console)
		c=curcons;
	if(c!=curcons)
		return;
	volatile unsigned short position = (25 * 80 + 80);
	outb(0x3D4, 0x0F);
	outb(0x3D5, (unsigned char)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (unsigned char )((position>>8)&0xFF));
}

struct renderer crtc_renderer = {
	crtc_scrolldown,
	0,
	crtc_update_cursor,
	crtc_clear,
	crtc_console_put,
	0, /* switch in */
	crtc_clear_cursor,
};

void crtc_init_console(struct vterm *con)
{
	if(!crtc_first_init) {
		crtc_first_init=1;
		memset((void *)VIDEO_MEMORY, 0, 80*25*2);
	}
	con->video = (char *)VIDEO_MEMORY;
	con->bd = 2;
	con->fw = con->fh=1;
	con->f=15;
	con->scrollb=25;
	con->w=80;
	con->h=25;
	char *vmem = con->vmem;
	if(!vmem && (kernel_state_flags&KSF_MMU))
		vmem = (char *)kmalloc(80*25*2+4);
	if(!vmem)
		panic(0, "Couldn't allocate virtual video memory (mmu is needed)");
	if(con != curcons)
		con->cur_mem=vmem;
	else
		con->cur_mem = con->video;
	con->vmem=vmem;
	con->rend = &crtc_renderer;
	crtc_clear(con);
}
