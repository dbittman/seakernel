#include <sea/tm/kthread.h>
#include <sea/tm/thread.h>
#include <sea/dm/block.h>
int block_elevator_main(struct kthread *kt, void *arg)
{
	blockdevice_t *dev = arg;
	const int max = 8;
	char *buf = kmalloc(dev->blksz * max);
	while(!kthread_is_joining(kt)) {
		struct queue_item *qi;
		if((qi = queue_dequeue_item(&dev->wq))) {
			struct ioreq *req = qi->ent;
			size_t count = req->count;
			size_t block = req->block;
			while(count) {
				int this = 8;
				if(this > count)
					this = count;
				assert(req->direction == READ);
				int ret = dm_do_block_rw_multiple(req->direction, req->dev, block, buf, this, dev);
				if(ret == dev->blksz * this) {
					for(int i=0;i<this;i++) {
						struct buffer *buffer = buffer_create(dev, block + i, 0, buf + i * dev->blksz);
						dm_block_cache_insert(dev, block + i, buffer, 0);
						buffer_put(buffer);
					}
				} else {
					req->flags |= IOREQ_FAILED;
				}
				block+=this;
				count-=this;
			}
			req->flags |= IOREQ_COMPLETE;
			tm_blocklist_wakeall(&req->blocklist);
			ioreq_put(req);
		} else {
			tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
			tm_schedule();
		}
	}
	return 0;
}


