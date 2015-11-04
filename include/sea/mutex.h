#ifndef NEW_MUTEX_H
#define NEW_MUTEX_H

#include <sea/types.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <sea/tm/blocking.h>
#define MUTEX_MAGIC 0xDEADBEEF
#define MT_ALLOC 1

struct thread;
struct mutex {
	struct blocklist blocklist;
	unsigned magic;
	_Atomic bool lock;
	unsigned flags;
	struct thread *owner;
	char *owner_file;
	int owner_line;
};

void __mutex_acquire(struct mutex *m,char*,int);
void __mutex_release(struct mutex *m,char*,int);
struct mutex *mutex_create(struct mutex *m, unsigned);
void mutex_destroy(struct mutex *m);

#define mutex_acquire(m) __mutex_acquire(m, __FILE__, __LINE__)
#define mutex_release(m) __mutex_release(m, __FILE__, __LINE__)

#endif

