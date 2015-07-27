#include <sea/dm/block.h>
#include <sea/mm/kmalloc.h>
struct ioreq *ioreq_create(blockdevice_t *bd, dev_t dev, uint64_t start, size_t count)
{
	struct ioreq *req = kmalloc(sizeof(*req));
	req->block = start;
	req->count = count;
	req->direction = READ;
	req->bd = bd;
	req->flags = 0;
	req->refs = 1;
	req->dev = dev; // TODO: get rid of dev;
	ll_create(&req->blocklist);
	return req;
}

void ioreq_put(struct ioreq *req)
{
	if(sub_atomic(&req->refs, 1) == 0)
		kfree(req);
}


