/* Basic, non-modularized access to the keyboard. */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <keyb.h>
#include <keymap.h>
int is_shift=0;
const unsigned char kbdus[];
const unsigned char kbduss[];

unsigned process(unsigned key)
{
	static unsigned short kbd_status, saw_break_code;
	unsigned char temp;
	if(key >= 0x80)
	{
		saw_break_code = 1;
		key &= 0x7F;
	}
	if(saw_break_code)
	{
		if(key == RAW1_LEFT_SHIFT || key == RAW1_RIGHT_SHIFT)
			kbd_status &= ~KBD_META_SHIFT;
		saw_break_code = 0;
		return 0;
	}
	if(key == RAW1_LEFT_SHIFT || key == RAW1_RIGHT_SHIFT)
	{
		kbd_status |= KBD_META_SHIFT;
		return 0;
	}
	temp = kbdus[key];
	if(temp == 0)
		return temp;
	if(kbd_status & KBD_META_SHIFT)
		is_shift=1;
	else
		is_shift=0;
	return temp;
}

void kb_int_handler()
{
	unsigned char scancode;
	/* Read from the keyboard's data buffer */
	scancode = inb(0x60);
	unsigned char ct;
	/* Convert actually processes break codes, and ctrl and shift (and later alt) and LEDs */
	char ym = process(scancode);
	super_cli();
	if (scancode & 0x80)
		is_shift=0;
	else
	{
		/* Normal characters */
		if(!is_shift)
			ct = kbdus[scancode];
		else
			ct = kbduss[scancode];
		if(!ct) 
			return;
		if(curcons->inpos < 253) {
			/* Add to stdin for the current console */
			if(ym)
			{
				curcons->input[curcons->inpos] = ct;
				curcons->inpos++;
			}
		}
		if(ym && ct != '\b' && curcons->term.c_lflag & ECHO)
			curcons->rend.putch(curcons, ct);
	}
}

void kb_install() {
	register_interrupt_handler(IRQ1, &kb_int_handler);
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

const unsigned char kbdus[] =
{
	/* 00 */0,	27,	'1',	'2',	'3',	'4',	'5',	'6',
	/* 08 */'7',	'8',	'9',	'0',	'-',	'=',	'\b',	'\t',
	/* 10 */'q',	'w',	'e',	'r',	't',	'y',	'u',	'i',
	/* 1Dh is left Ctrl */
	/* 18 */'o',	'p',	'[',	']',	'\n',	0,	'a',	's',
	/* 20 */'d',	'f',	'g',	'h',	'j',	'k',	'l',	';',
	/* 2Ah is left Shift */
	/* 28 */'\'',	'`',	0,	'\\',	'z',	'x',	'c',	'v',
	/* 36h is right Shift */
	/* 30 */'b',	'n',	'm',	',',	'.',	'/',	0,	0,
	/* 38h is left Alt, 3Ah is Caps Lock */
	/* 38 */0,	' ',	0,	KEY_F1,	KEY_F2,	KEY_F3,	KEY_F4,	KEY_F5,
	/* 45h is Num Lock, 46h is Scroll Lock */
	/* 40 */KEY_F6,	KEY_F7,	KEY_F8,	KEY_F9,	KEY_F10,0,	0,	KEY_HOME,
	/* 48 */KEY_UP,	KEY_PGUP,'-',	KEY_LFT,'5',	KEY_RT,	'+',	KEY_END,
	/* 50 */KEY_DN,	KEY_PGDN,KEY_INS,KEY_DEL,0,	0,	0,	KEY_F11,
	/* 58 */KEY_F12
};

const unsigned char kbduss[] =
{
	/* 00? */0,	27,	'!',	'@',	'#',	'$',	'%',	'^',
	/* 08 */'&',	'*',	'(',	')',	'_',	'+',	'\b',	'\t',
	/* 10 */'Q',	'W',	'E',	'R',	'T',	'Y',	'U',	'I',
	/* 1Dh is left Ctrl */
	/* 18 */'O',	'P',	'{',	'}',	'\n',	0,	'A',	'S',
	/* 20 */'D',	'F',	'G',	'H',	'J',	'K',	'L',	':',
	/* 2Ah is left Shift */
	/* 28 */'"',	'~',	0,	'|',	'Z',	'X',	'C',	'V',
	/* 36h is right Shift */
	/* 30 */'B',	'N',	'M',	'<',	'>',	'?',	0,	0,
	/* 38h is left Alt, 3Ah is Caps Lock */
	/* 38 */0,	' ',	0,	KEY_F1,	KEY_F2,	KEY_F3,	KEY_F4,	KEY_F5,
	/* 45h is Num Lock, 46h is Scroll Lock */
	/* 40 */KEY_F6,	KEY_F7,	KEY_F8,	KEY_F9,	KEY_F10,0,	0,	KEY_HOME,
	/* 48 */KEY_UP,	KEY_PGUP,'-',	KEY_LFT,'5',	KEY_RT,	'+',	KEY_END,
	/* 50 */KEY_DN,	KEY_PGDN,KEY_INS,KEY_DEL,0,	0,	0,	KEY_F11,
	/* 58 */KEY_F12
};
