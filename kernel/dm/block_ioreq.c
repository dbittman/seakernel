#include <sea/dm/block.h>
#include <sea/dm/blockdev.h>
#include <sea/mm/kmalloc.h>
#include <sea/tm/blocking.h>
#include <stdatomic.h>
struct ioreq *ioreq_create(struct blockdev *bd, int direction, uint64_t start, size_t count)
{
	struct ioreq *req = kmalloc(sizeof(*req));
	req->block = start;
	req->count = count;
	req->direction = direction;
	req->bd = bd;
	req->flags = 0;
	req->refs = 1;
	blocklist_create(&req->blocklist, 0, "ioreq");
	return req;
}

void ioreq_put(struct ioreq *req)
{
	if(atomic_fetch_sub(&req->refs, 1) == 1)
		kfree(req);
}

