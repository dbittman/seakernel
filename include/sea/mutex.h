#ifndef NEW_MUTEX_H
#define NEW_MUTEX_H

#include <sea/types.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <sea/tm/blocking.h>
#define MUTEX_MAGIC 0xDEADBEEF
#define MT_ALLOC 1

typedef struct {
	struct blocklist blocklist;
	unsigned magic;
	_Atomic bool lock;
	unsigned flags;
	long pid;
	char *owner_file;
	int owner_line;
} mutex_t;

void __mutex_acquire(mutex_t *m,char*,int);
void __mutex_release(mutex_t *m,char*,int);
mutex_t *mutex_create(mutex_t *m, unsigned);
void mutex_destroy(mutex_t *m);

#define mutex_acquire(m) __mutex_acquire(m, __FILE__, __LINE__)
#define mutex_release(m) __mutex_release(m, __FILE__, __LINE__)

#endif

