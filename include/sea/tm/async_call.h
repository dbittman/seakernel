#ifndef __SEA_TM_ASYNC_CALL_H
#define __SEA_TM_ASYNC_CALL_H

#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>

#define ASYNC_CALL_PRIORITY_HIGH   100
#define ASYNC_CALL_PRIORITY_MEDIUM 50
#define ASYNC_CALL_PRIORITY_LOW    10
#define ASYNC_CALL_PRIORITY_MIN    0

struct async_call {
	void (*func)(unsigned long data);
	int flags;
	int priority;
	void *queue;
	unsigned long data;
};

static inline struct async_call *async_call_create(struct async_call *ac, int flags,
		void (*func)(unsigned long),
		unsigned long data, int priority)
{
	if(!ac) {
		panic(0, "async_call may not be allocated at runtime");
	} else {
		ac->flags = flags;
	}
	ac->func = func;
	ac->queue = 0;
	ac->priority = priority;
	ac->data = data;
	return ac;
}

static inline void async_call_execute(struct async_call *ac)
{
	ac->func(ac->data);
}

static inline void async_call_destroy(struct async_call *ac)
{
}

#endif

