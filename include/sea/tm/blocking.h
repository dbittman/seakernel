#ifndef __SEA_TM_BLOCKING_H
#define __SEA_TM_BLOCKING_H

#include <sea/lib/linkedlist.h>
#include <sea/spinlock.h>
#include <sea/tm/async_call.h>

#define BLOCKLIST_ALLOC 1

struct blocklist {
	struct linkedlist list;
	struct spinlock lock;
	int flags;
	const char *name;
};

void tm_blocklist_wakeall(struct blocklist *blocklist);
void tm_blocklist_wakeone(struct blocklist *blocklist);
int tm_thread_block_timeout(struct blocklist *blocklist, time_t microseconds);
int tm_thread_block_schedule_work(struct blocklist *blocklist,
		int state, struct async_call *work);
int tm_thread_block_confirm(struct blocklist *blocklist, int state,
		bool (*cfn)(void *), void *data);
int tm_thread_block(struct blocklist *blocklist, int state);
struct blocklist *blocklist_create(struct blocklist *list, int flags, const char *);
void blocklist_destroy(struct blocklist *list);
#endif

