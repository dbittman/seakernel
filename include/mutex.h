#ifndef NEW_MUTEX_H
#define NEW_MUTEX_H

#define MUTEX_MAGIC 0xDEADBEEF
#define MT_ALLOC 1
#define MT_NOSCHED 2

/* this bit in lock is set when a task that currently owns the mutex
 * handles an interrupt, and inside that interrupt it locks the same
 * mutex again. This is legal, but it's handled in a special way */
#define MT_LCK_INT 2

typedef struct {
	unsigned magic;
	unsigned lock;
	unsigned flags;
	int pid;
} mutex_t;

void __mutex_acquire(mutex_t *m,char*,int);
void __mutex_release(mutex_t *m,char*,int);
mutex_t *mutex_create(mutex_t *m, unsigned);
void mutex_destroy(mutex_t *m);

#define mutex_acquire(m) __mutex_acquire(m, __FILE__, __LINE__)
#define mutex_release(m) __mutex_release(m, __FILE__, __LINE__)

#endif
