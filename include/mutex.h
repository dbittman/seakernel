#ifndef NEW_MUTEX_H
#define NEW_MUTEX_H

#define MUTEX_MAGIC 0xDEADBEEF
#define MT_ALLOC 1

typedef struct {
	unsigned magic;
	unsigned lock;
	unsigned flags;
	int pid;
} mutex_t;

void mutex_acquire(mutex_t *m);
void mutex_release(mutex_t *m);
mutex_t *mutex_create(mutex_t *m);
void mutex_destroy(mutex_t *m);

#endif
