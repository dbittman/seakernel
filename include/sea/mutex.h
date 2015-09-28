#ifndef NEW_MUTEX_H
#define NEW_MUTEX_H

#include <sea/types.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <sea/lib/linkedlist.h>
#define MUTEX_MAGIC 0xDEADBEEF
#define MT_ALLOC 1

/* this bit in lock is set when a task that currently owns the mutex
 * handles an interrupt, and inside that interrupt it locks the same
 * mutex again. This is legal, but it's handled in a special way */
#define MT_LCK_INT 2

typedef struct {
	unsigned magic;
	_Atomic unsigned alignas(8) lock;
	unsigned flags;
	long pid;
	char *owner_file;
	int owner_line;
	struct linkedlist blocklist;
} mutex_t;

void __mutex_acquire(mutex_t *m,char*,int);
void __mutex_release(mutex_t *m,char*,int);
mutex_t *mutex_create(mutex_t *m, unsigned);
void mutex_destroy(mutex_t *m);

#define mutex_acquire(m) __mutex_acquire(m, __FILE__, __LINE__)
#define mutex_release(m) __mutex_release(m, __FILE__, __LINE__)

int mutex_is_locked(mutex_t *m);

#endif

