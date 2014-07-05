/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

/* Adapted from linux 0.01 - */
/* Added support for length limiting: July 2014 - Daniel Bittman */
#include <sea/types.h>
#include <sea/config.h>
#include <stdarg.h>
#include <sea/kernel.h>
#include <sea/string.h>
#include <sea/tty/terminal.h>
#include <sea/vsprintf.h>
#include <sea/dm/dev.h>
#define LOGL_SERIAL 0
#define LOGL_LOGTTY 1

/* we use this so that we can do without the ctype library */
#define is_digit(c)	((c) >= '0' && (c) <= '9')

static int skip_atoi(const char **s)
{
	int i=0;

	while (is_digit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define SMALL	64		/* use 'abcdef' instead of 'ABCDEF' */

static char * number(int allowed_length, char * str, long num, int base, int size, int precision ,int type)
{
	char c,sign,tmp[72];
	const char *digits="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	if (type&SMALL) digits="0123456789abcdefghijklmnopqrstuvwxyz";
	if (type&LEFT) type &= ~ZEROPAD;
	if (base<2 || base>36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ' ;
	if (type&SIGN && num<0) {
		sign='-';
		num = -num;
	} else
		sign=(type&PLUS) ? '+' : ((type&SPACE) ? ' ' : 0);
	if (sign) size--;
	if (type&SPECIAL){
		if (base==16) size -= 2;
		else if (base==8) size--;
	}
	i=0;
	if (num==0)
		tmp[i++]='0';
	else while (num!=0) {
		tmp[i++]=digits[(unsigned long)num % base];
		num = (unsigned long)num / base;
	}
	if (i>precision) precision=i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0) {
			if(allowed_length == 0)
				return str;
			*str++ = ' ';
			allowed_length--;
		}
	if (sign) {
		if(allowed_length == 0)
			return str;
		*str++ = sign;
		allowed_length--;
	}
	if (type&SPECIAL) {
		if (base==8) {
			if(allowed_length == 0)
				return str;
			*str++ = '0';
			allowed_length--;
		}
		else if (base==16) {
			if(allowed_length == 0)
				return str;
			*str++ = '0';
			if(--allowed_length == 0)
				return str;
			*str++ = digits[33];
			allowed_length--;
		}
	}
	if (!(type&LEFT))
		while(size-->0) {
			if(allowed_length == 0)
				return str;
			*str++ = c;
			allowed_length--;
		}
	
	while(i<precision--) {
		if(allowed_length == 0)
			return str;
		*str++ = '0';
		allowed_length--;
	}

	while(i-->0) {
		if(allowed_length == 0)
			return str;
		*str++ = tmp[i];
		allowed_length--;
	}
	
	while(size-->0) {
		if(allowed_length == 0)
			return str;
		*str++ = ' ';
		allowed_length--;
	}
	return str;
}

int vsnprintf(int size, char *buf, const char *fmt, va_list args)
{
	int len;
	int i;
	char *str, *newstr;
	char *s;
	int *ip;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	int used_size=1; /* for null character */
	
	if(size == 0) return 0;
	*buf=0;
	if(size == 1) return 1;

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			if(used_size >= size)
				break;
			*str++ = *fmt;
			used_size++;
			continue;
		}
			
		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}
		
		/* get field width */
		field_width = -1;
		if (is_digit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;	
			if (is_digit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0) {
					if(used_size >= size)
						goto exit_location;
					*str++ = ' ';
					used_size++;
				}
			
			if(used_size >= size)
				goto exit_location;
			*str++ = (unsigned char) va_arg(args, int);
			used_size++;
			while (--field_width > 0) {
				if(used_size >= size)
					goto exit_location;
				*str++ = ' ';
				used_size++;
			}
			break;
		case 's':
			s = va_arg(args, char *);
			len = strlen(s);
			if (precision < 0)
				precision = len;
			else if (len > precision)
				len = precision;

			if (!(flags & LEFT))
				while (len < field_width--) {
					if(used_size >= size)
						goto exit_location;
					*str++ = ' ';
					used_size++;
				}
			for (i = 0; i < len; ++i) {
				if(used_size >= size)
					goto exit_location;
				*str++ = *s++;
				used_size++;
			}
			while (len < field_width--) {
				if(used_size >= size)
					goto exit_location;
				*str++ = ' ';
				used_size++;
			}
			break;

		case 'o':
			newstr = number(size-used_size, str, va_arg(args, unsigned long), 8,
				field_width, precision, flags);
			used_size += newstr - str; str = newstr;
			if(used_size >= size)
				goto exit_location;
			break;

		case 'p':
			if (field_width == -1) {
				field_width = 8;
				flags |= ZEROPAD;
			}
			newstr = number(size-used_size, str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			used_size += newstr - str; str = newstr;
			if(used_size >= size)
				goto exit_location;
			break;

		case 'x':
			flags |= SMALL;
		case 'X':
			newstr = number(size-used_size, str, va_arg(args, unsigned long), 16,
				field_width, precision, flags);
			used_size += newstr - str; str = newstr;
			if(used_size >= size)
				goto exit_location;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			newstr = number(size-used_size, str, va_arg(args, unsigned long), 10,
				field_width, precision, flags);
			used_size += newstr - str; str = newstr;
			if(used_size >= size)
				goto exit_location;
			break;

		case 'n':
			ip = va_arg(args, int *);
			*ip = (str - buf);
			break;

		default:
			if (*fmt != '%') {
				if(used_size >= size)
					goto exit_location;
				*str++ = '%';
				used_size++;
			}
			if (*fmt) {
				if(used_size >= size)
					goto exit_location;
				*str++ = *fmt;
				used_size++;
			}
			else
				--fmt;
			break;
		}
	}
exit_location:
	*str=0;
	return used_size;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{ 
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(size, buf, fmt, args);
	va_end(args);
	return len;
}
void serial_console_puts_nolock(int port, char *s);
void serial_console_puts(int port, char *s);
/* This WILL print to the screen */
void kprintf(const char *fmt, ...)
{
	char printbuf[2024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(2024, printbuf, fmt, args);
	console_kernel_puts(printbuf);
	if(kernel_state_flags & KSF_PANICING)
		serial_console_puts_nolock(0, printbuf);
	else
		serial_console_puts(0, printbuf);
	va_end(args);
}

/* Will print to screen only if the printlevel is above the log-level */
void printk(int l, const char *fmt, ...)
{
	char printbuf[2024];
	va_list args;
	int i=0;
	va_start(args, fmt);
	vsnprintf(2024, printbuf, fmt, args);
	if(l >= LOGL_SERIAL)
		serial_console_puts(0, printbuf);
	if(l >= LOGL_LOGTTY && log_console)
		console_puts(log_console, printbuf);
	if(l >= PRINT_LEVEL)
		console_kernel_puts(printbuf);
	va_end(args);
}

/* Will print to screen only if the printlevel is above the log-level */
void printk_safe(int l, const char *fmt, ...)
{
	char printbuf[2024];
	va_list args;
	int i=0;
	va_start(args, fmt);
	vsnprintf(2024, printbuf, fmt, args);
	if(l >= LOGL_SERIAL)
		serial_console_puts_nolock(0, printbuf);
	if(l >= LOGL_LOGTTY && log_console)
		console_puts(log_console, printbuf);
	if(l >= PRINT_LEVEL)
		console_kernel_puts(printbuf);
	va_end(args);
}

