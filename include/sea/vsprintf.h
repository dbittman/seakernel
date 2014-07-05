#ifndef VSPRINTF_H
#define VSPRINTF_H

#include <stdarg.h>
#include <sea/types.h>

int vsnprintf(int size, char *buf, const char *fmt, va_list args);
int snprintf(char *buf, size_t size, const char *fmt, ...);
void kprintf(const char *fmt, ...);
void printk(int l, const char *fmt, ...);
void printk_safe(int l, const char *fmt, ...);

#endif

