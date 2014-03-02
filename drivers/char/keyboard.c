#include <kernel.h>
#include <isr.h>
#include <keymap.h>
#include <modules/keyboard.h>
#include <console.h>
#include <sig.h>
#include <task.h>
#include <symbol.h>
#include <sea/cpu/interrupt.h>

int is_ctrl=0, is_alt=0, is_shift=0, is_altgr=0;
int capslock, slock;
unsigned short *(*_keymap_callback)(int, int, int, int) = 0;

void flush_port()
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

void enqueue_char(char c)
{
	if(c == 3 && (curcons->term.c_lflag & ICANON))
	{
		ttyx_ioctl(curcons->tty, 19, SIGINT);
		return;
	}
	if(c == 26 && (curcons->term.c_lflag & ICANON))
	{
		ttyx_ioctl(curcons->tty, 19, SIGTSTP);
		return;
	}
	if(c == 28 && (curcons->term.c_lflag & ICANON))
	{
		ttyx_ioctl(curcons->tty, 19, SIGQUIT);
		return;
	}
	ttyx_ioctl(curcons->tty, 17, c);
}

void enqueue_string(char *c)
{
	while(c && *c)
	{
		enqueue_char(*c);
		c++;
	}
}

void function_keys(int value, int release)
{
	if(release) return;
	if(value < MAX_NR_FUNC)
	{
		if (func_table[value])
		{
			char *c = (func_table[value]);
			enqueue_string(c);
		}
	}
}

void norm_keys(int value, int release)
{
	if(release)
		return;
	switch(value)
	{
		case 0x7f:
			enqueue_char('\b');
			break;
		default:
			enqueue_char(value);
			break;
	}
}

void control_keys(int value, int release)
{
	if(release) return;
	/* RAW: Nothing */
	switch(value)
	{
		case K_ENTER:
			enqueue_char('\r');
			break;
		case K_CAPS:
			capslock=!capslock;
			break;
		case 9:
			slock = !slock;
			break;
	}
}

static void applkey(int key, char mode)
{
	static char buf[] = { 0x1b, 'O', 0x00, 0x00 };
	
	buf[1] = (mode ? 'O' : '[');
	buf[2] = key;
	enqueue_string(buf);
}

static void k_cur(unsigned char value, char up_flag)
{
	static const char cur_chars[] = "BDCA";
	
	if (up_flag)
		return;
	int mode = 0;//getmode: VC_CKMODE
	applkey(cur_chars[value], mode);
}

void special_keys(int value, int release)
{
	if(release) return;
	switch(value)
	{
		case 2:
			k_cur(0, release);
			break;
		case 4:
			k_cur(1, release);
			break;
		case 6:
			k_cur(2, release);
			break;
		case 8:
			k_cur(3, release);
			break;
	}
}

void letter_keys(int value, int release)
{
	if(release)
		return;
	enqueue_char(value);
}

void modifier_keys(int value, int release)
{
	switch(value)
	{
		case 0:
			is_shift = !release;
			break;
		case 2:
			is_ctrl = !release;
			break;
		case 3:
			is_alt = !release;
			break;
	}
}

/* x86_keycodes, raw and to_utf8 borrowed from linux */
static const unsigned short x86_keycodes[256] =
{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
80, 81, 82, 83, 84,118, 86, 87, 88,115,120,119,121,112,123, 92,
284,285,309,  0,312, 91,327,328,329,331,333,335,336,337,338,339,
367,288,302,304,350, 89,334,326,267,126,268,269,125,347,348,349,
360,261,262,263,268,376,100,101,321,316,373,286,289,102,351,355,
103,104,105,275,287,279,258,106,274,107,294,364,358,363,362,361,
291,108,381,281,290,272,292,305,280, 99,112,257,306,359,113,114,
264,117,271,374,379,265,266, 93, 94, 95, 85,259,375,260, 90,116,
377,109,111,277,278,282,283,295,296,297,299,300,301,293,303,307,
308,310,313,314,315,317,318,319,320,357,322,323,324,325,276,330,
332,340,365,342,343,344,345,346,356,270,341,368,369,370,371,372 };

int raw(int code, int release)
{
	if (code > 255)
		return -1;
	
	code = x86_keycodes[code];
	if (!code)
		return -1;
	
	if (code & 0x100)
		enqueue_char(0xe0);
	enqueue_char((code & 0x7f) | (release << 7));
	return 0;
}

unsigned short *get_keymap(int shift, int alt, int ctrl, int altgr)
{
	if(_keymap_callback)
		return _keymap_callback(shift, alt, ctrl, altgr);
	int a = 0;
	if(alt)
		a+=8;
	if(ctrl)
		a+=4;
	if(shift)
		a+=1;
	if(altgr)
		a+=2;
	return key_maps[a];
}

static void to_utf8(unsigned int c)
{
	if (c < 0x80)
		/*  0******* */
		enqueue_char(c);
	else if (c < 0x800) {
		/* 110***** 10****** */
		enqueue_char(0xc0 | (c >> 6));
		enqueue_char(0x80 | (c & 0x3f));
	} else if (c < 0x10000) {
		if (c >= 0xD800 && c < 0xE000)
			return;
		if (c == 0xFFFF)
			return;
		/* 1110**** 10****** 10****** */
		enqueue_char(0xe0 | (c >> 12));
		enqueue_char(0x80 | ((c >> 6) & 0x3f));
		enqueue_char(0x80 | (c & 0x3f));
	} else if (c < 0x110000) {
		/* 11110*** 10****** 10****** 10****** */
		enqueue_char(0xf0 | (c >> 18));
		enqueue_char(0x80 | ((c >> 12) & 0x3f));
		enqueue_char(0x80 | ((c >> 6) & 0x3f));
		enqueue_char(0x80 | (c & 0x3f));
	}
}

int try_console_switch(int code)
{
	if(!is_ctrl)
		return 0;
	unsigned short *map = get_keymap(0, 0, 0, 0);
	code = map[code];
	int type = code >> 8;
	if(type < 0xF0)
		return 0;
	type -= 0xF0;
	if(type == 0)
	{
		if((code & 0xff) >= '0' && (code & 0xff) <= '9')
		{
			ttyx_ioctl((code & 0xff) - '0', 27, 0);
			return 1;
		}
	}
	return 0;
}
#define BREAKER1 69
#define BREAKER0 225
unsigned char last_sc=0;
unsigned char key_stack[64];
int ks_idx=0;
int keyboard_int_stage1()
{
	unsigned char scancode = inb(0x60);
	int x = add_atomic(&ks_idx, 1)-1;
	if(ks_idx > 63) {
		sub_atomic(&ks_idx, 1);
		return 0;
	}
	key_stack[x] = scancode;
	return 0;
}

int keyboard_int_stage2()
{
	int x = sub_atomic(&ks_idx, 1);
	if(x < 0) {
		ks_idx=0;
		return 0;
	}
	unsigned char scancode = key_stack[x];
	if(scancode == BREAKER1 && last_sc == BREAKER0)
	{
		asm("int $0x3");
	}
	last_sc = scancode;
	int release = (scancode > 127) ? 1 : 0;
	if(release) scancode -= 128;
	unsigned short *map = get_keymap(is_shift, is_alt, is_ctrl, is_altgr);
	if(!release)
		if(try_console_switch(scancode))
			return 0;
	if(!map)
		return 0;
	unsigned code = map[scancode];
	int type = code >> 8;
	if(type > 0xF0 && (type-0xF0) == 0xb)
	{
		map = get_keymap(is_shift ^ capslock, is_alt, is_ctrl, is_altgr);
		code = map[scancode];
		type = code >> 8;
	}
	int value = code & 0xFF;
	if(type < 0xF0) {
		if (!release)
			to_utf8(code);
		return 0;
	}
	type -= 0xF0;
	switch(type)
	{
		case 0: /* Keys like numbers, don't respond to capslock key */
			norm_keys(value, release);
			break;
		case 1: /* F1 - F12 */
			function_keys(value, release);
			break;
		case 2: /* Enter, etc */
			control_keys(value, release);
			break;
		case 3: /* Arrow, ins, del, etc */
			special_keys(value, release);
			break;
		case 7: /* ctrl, alt, etc */
			modifier_keys(value, release);
			break;
		case 0xb: /* A, B, C */
			letter_keys(value, release);
			break;
		default:
			if(!release) printk(0, "[keyboard]: unknown scancode: %d-> %x\n", scancode, (unsigned)code);
	}
	return 0;
}

void do_keyboard_int()
{
	keyboard_int_stage2();
	//flush_port();
}

void set_keymap_callback(addr_t ptr)
{
	_keymap_callback = (unsigned short *(*)(int, int, int, int))ptr;
}

addr_t get_keymap_callback()
{
	return (addr_t)_keymap_callback;
}
int irqk=0;
int module_install()
{
	printk(1, "[keyboard]: Driver loading\n");
	is_shift=0;
	is_alt=0;
	ks_idx=0;
	is_ctrl=0;
	is_altgr=0;
	capslock=0;
	_keymap_callback=0;
	loader_add_kernel_symbol(set_keymap_callback);
	loader_add_kernel_symbol(get_keymap_callback);
	irqk = arch_interrupt_register_handler(IRQ1, (isr_t)&keyboard_int_stage1, (isr_t)&keyboard_int_stage2);
	flush_port();
	printk(1, "[keyboard]: initialized keyboard\n");
	return 0;
}

int module_tm_exit()
{
	flush_port();
	printk(1, "[keyboard]: Restoring old handler\n");
	arch_interrupt_unregister_handler(IRQ1, irqk);
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
