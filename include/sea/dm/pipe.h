#ifndef __SEA_DM_PIPE_H
#define __SEA_DM_PIPE_H

#include <ll.h>
#include <mutex.h>
#include <types.h>

#define PIPE_NAMED 1
typedef struct pipe_struct {
	volatile unsigned pending;
	volatile unsigned write_pos, read_pos;
	volatile char *buffer;
	volatile off_t length;
	mutex_t *lock;
	char type;
	volatile int count, wrcount;
	struct llist *read_blocked, *write_blocked;
} pipe_t;

#endif
