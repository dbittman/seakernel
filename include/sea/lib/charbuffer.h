#ifndef __SEA_LIB_CHARBUFFER_H
#define __SEA_LIB_CHARBUFFER_H

#include <sea/types.h>
#include <sea/mutex.h>
#include <sea/tm/blocking.h>
#define CHARBUFFER_ALLOC 1
#define CHARBUFFER_LOCKLESS 2
#define CHARBUFFER_OVERWRITE 4

struct charbuffer {
	unsigned char *buffer;
	size_t head, tail, cap;
	_Atomic size_t count;
	_Atomic int eof;
	int flags;
	struct mutex lock;
	struct blocklist readers;
	struct blocklist writers;
};

struct charbuffer *charbuffer_create(struct charbuffer *cb, int flags, size_t cap);
void charbuffer_destroy(struct charbuffer *cb);
size_t charbuffer_read(struct charbuffer *cb, unsigned char *out, size_t length);
size_t charbuffer_write(struct charbuffer *cb, unsigned char *in, size_t length);

static inline size_t charbuffer_count(struct charbuffer *cb)
{
	return cb->count;
}

#endif

