#include <sea/tm/kthread.h>
#include <sea/tm/blocking.h>
#include <sea/tm/thread.h>
#include <sea/dm/block.h>
#include <sea/dm/blockdev.h>
#include <stdatomic.h>
void dm_block_cache_reclaim(void);
int block_elevator_main(struct kthread *kt, void *arg)
{
	struct inode *node = arg;
	struct blockdev *__bd = node->devdata;
	struct blockctl *ctl = __bd->ctl;
	const int max = 8;
	char *buf = kmalloc(ctl->blocksize * max);
	while(!kthread_is_joining(kt)) {
		struct queue_item *qi;
		if((qi = queue_dequeue_item(&ctl->wq))) {
			struct ioreq *req = qi->ent;
			size_t count = req->count;
			size_t block = req->block;

			if(req->direction == READ) {
				while(count) {
					int this = 8;
					if(this > (int)count)
						this = count;
					ssize_t ret = ctl->rw(req->direction, node, block + req->bd->partbegin, buf,
							this);
					if(ret == ctl->blocksize * this) {
						for(int i=0;i<this;i++) {
							struct buffer *buffer = buffer_create(req->bd, node->phys_dev,
									block + i, 0, buf + i * ctl->blocksize);
							atomic_fetch_or(&buffer->flags, BUFFER_LOCKED);
							dm_block_cache_insert(req->bd, block + i, buffer, 0);
							buffer_put(buffer);
						}
					} else {
						req->flags |= IOREQ_FAILED;
					}
					block+=this;
					count-=this;
				}
			} else {
				while(count) {
					int this = 8;
					if(this > (int)count)
						this = count;

					for(int i=0;i<this;i++) {
						struct buffer *buffer = dm_block_cache_get(req->bd, block + i);
						assert(buffer);
						memcpy(buf + i * ctl->blocksize, buffer->data, ctl->blocksize);
						atomic_fetch_and(&buffer->flags, ~(BUFFER_DIRTY | BUFFER_WRITEPENDING)); //Should we wait to do this?
						buffer_put(buffer);
					}

					ssize_t ret = ctl->rw(WRITE, node, block + req->bd->partbegin, buf, this);
					if(ret != ctl->blocksize * this) {
						req->flags |= IOREQ_FAILED;
						break;
					}
					block += this;
					count -= this;
				}
			}
			req->flags |= IOREQ_COMPLETE;
			tm_blocklist_wakeall(&req->blocklist);
			ioreq_put(req);
		} else {
			if(hash_length(&ctl->cache) && (hash_count(&ctl->cache) * 100) / hash_length(&ctl->cache) > 300) {
				dm_block_cache_reclaim();
				tm_schedule();
			} else {
				tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
			}
		}
	}
	kfree(buf);
	return 0;
}

