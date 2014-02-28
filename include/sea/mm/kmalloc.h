#ifndef __SEA_MM_KMALLOC_H
#define __SEA_MM_KMALLOC_H

#include <types.h>

void kfree(void * pt);
void *__kmalloc(size_t s, char *, int);
void *__kmalloc_a(size_t s, char *, int);
void *__kmalloc_ap(size_t s, addr_t *, char *, int);
void *__kmalloc_p(size_t s, addr_t *, char *, int);

#define kmalloc(a) __kmalloc(a, __FILE__, __LINE__)
#define kmalloc_a(a) __kmalloc_a(a, __FILE__, __LINE__)
#define kmalloc_p(a,x) __kmalloc_p(a, x, __FILE__, __LINE__)
#define kmalloc_ap(a,x) __kmalloc_ap(a, x, __FILE__, __LINE__)

#endif
