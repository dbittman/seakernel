#include <sea/dm/dev.h>
#include <sea/tm/blocking.h>
#include <sea/fs/inode.h>
#include <sea/fs/pipe.h>
#include <sea/tm/process.h>
#include <sea/fs/file.h>
#include <sea/cpu/interrupt.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/tm/blocking.h>
static struct pipe *fs_pipe_create(void)
{
	struct pipe *pipe = (struct pipe *)kmalloc(sizeof(struct pipe));
	pipe->length = PIPE_SIZE;
	pipe->buffer = (char *)kmalloc(PIPE_SIZE);
	mutex_create(&pipe->lock, 0);
	pipe->wrcount=1;
	pipe->recount=1;
	return pipe;
}

static void __pipe_close(struct file *file)
{
	struct pipe *pipe = file->inode->devdata;
	mutex_acquire(&pipe->lock);
	assert(pipe);
	if(file->flags & _FWRITE) {
		int r = atomic_fetch_sub(&pipe->wrcount, 1);
		assert(r > 0);
	} else {
		int r = atomic_fetch_sub(&pipe->recount, 1);
		assert(r > 0);
	}
	tm_blocklist_wakeall(&file->inode->readblock);
	tm_blocklist_wakeall(&file->inode->writeblock);
	mutex_release(&pipe->lock);
}

static int __pipe_select(struct file *file, int rw)
{
	struct pipe *pipe = file->inode->devdata;
	if(!pipe) return 1;
	if(rw == READ) {
		if(!pipe->pending && pipe->wrcount > 0)
			return 0;
	} else if(rw == WRITE) {
		if(pipe->pending == PIPE_SIZE && pipe->recount > 0)
			return 0;
	}
	return 1;
}

static bool __release_lock(void *m)
{
	mutex_release((struct mutex *)m);
	return true;
}

static int __pipe_read(struct file *file, unsigned char *buffer, size_t length)
{
	struct pipe *pipe = file->inode->devdata;
	assert(pipe);
	size_t len = length;
	int ret=0;
	/* should we even try reading? (empty pipe with no writing processes=no) */
	if(!pipe->pending && pipe->wrcount == 0)
		return 0;
	if((file->flags & _FNONBLOCK) && !pipe->pending)
		return -EAGAIN;
	/* block until we have stuff to read */
	mutex_acquire(&pipe->lock);
	while(!pipe->pending && (pipe->wrcount>0)) {
		/* we need to block, but also release the lock. Disable interrupts
		 * so we don't schedule before we want to */
		int r = tm_thread_block_confirm(&file->inode->readblock,
				THREADSTATE_INTERRUPTIBLE, __release_lock, &pipe->lock);
		switch(r) {
			case -ERESTART:
				tm_blocklist_wakeall(&file->inode->writeblock);
				tm_blocklist_wakeall(&file->inode->readblock);
				return -ERESTART;
			case -EINTR:
				tm_blocklist_wakeall(&file->inode->writeblock);
				tm_blocklist_wakeall(&file->inode->readblock);
				return -EINTR;
		}
		mutex_acquire(&pipe->lock);
	}

	ret = pipe->pending > len ? len : pipe->pending;
	for(int i=0;i<ret;i++) {
		buffer[i] = pipe->buffer[pipe->read_pos % PIPE_SIZE];
		pipe->read_pos++;
	}
	pipe->pending -= ret;

	tm_blocklist_wakeall(&file->inode->writeblock);
	tm_blocklist_wakeall(&file->inode->readblock);
	
	mutex_release(&pipe->lock);
	return ret;
}

static int __pipe_write(struct file *file, unsigned char *initialbuffer, size_t totallength)
{
	struct pipe *pipe = file->inode->devdata;
	assert(pipe);
	/* allow for partial writes of the system page size. Thus, we wont
	 * have a process freeze because it tries to fill up the pipe in one
	 * shot. */
	unsigned char *buffer = initialbuffer;
	size_t length;
	size_t remain = totallength;
	size_t written = 0;
	mutex_acquire(&pipe->lock);
	while(remain) {
		length = PAGE_SIZE;
		if(length > remain)
			length = remain;
		/* we're writing to a pipe with no reading process! */
		if(pipe->recount == 0) {
			tm_blocklist_wakeall(&file->inode->readblock);
			tm_blocklist_wakeall(&file->inode->writeblock);
			mutex_release(&pipe->lock);
			tm_signal_send_thread(current_thread, SIGPIPE);
			return -EPIPE;
		}
		if((file->flags & _FNONBLOCK) && pipe->pending + totallength > PIPE_SIZE) {
			tm_blocklist_wakeall(&file->inode->readblock);
			tm_blocklist_wakeall(&file->inode->writeblock);
			mutex_release(&pipe->lock);
			if(written)
				return written;
			return -EAGAIN;
		}

		/* IO block until we can write to it */
		while((pipe->pending+length)>=PIPE_SIZE) {
			tm_blocklist_wakeall(&file->inode->readblock);
			int r = tm_thread_block_confirm(&file->inode->writeblock,
					THREADSTATE_INTERRUPTIBLE, __release_lock, &pipe->lock);
			if(r)
			switch(r) {
				case -ERESTART:
					tm_blocklist_wakeall(&file->inode->readblock);
					tm_blocklist_wakeall(&file->inode->writeblock);
					if(written)
						return written;
					return -ERESTART;
				case -EINTR:
					tm_blocklist_wakeall(&file->inode->readblock);
					tm_blocklist_wakeall(&file->inode->writeblock);
					if(written)
						return written;
					return -EINTR;
			}
			mutex_acquire(&pipe->lock);
		}
		for(unsigned i=0;i<length;i++) {
			pipe->buffer[pipe->write_pos % PIPE_SIZE] = buffer[i];
			pipe->write_pos++;
		}
		pipe->pending += length;
		/* now, unblock the tasks */
		tm_blocklist_wakeall(&file->inode->readblock);
		tm_blocklist_wakeall(&file->inode->writeblock);

		remain -= length;
		buffer += length;
	}
	mutex_release(&pipe->lock);
	return totallength;
}

static ssize_t __pipe_rw(int rw, struct file *file, off_t off, uint8_t *buffer, size_t len)
{
	if(rw == READ)
		return __pipe_read(file, buffer, len);
	else if(rw == WRITE)
		return __pipe_write(file, buffer, len);
	return -EIO;
}

static void __pipe_destroy(struct inode *node)
{
	struct pipe *pipe = node->devdata;
	kfree(pipe->buffer);
	mutex_destroy(&pipe->lock);
	kfree(pipe);
}

static struct kdevice __pipe_kdev = {
	.select = __pipe_select,
	.close = __pipe_close,
	.rw = __pipe_rw,
	.create = 0,
	.destroy = __pipe_destroy,
	.open = 0,
	.ioctl = 0,
	.name = "pipe",
};

int sys_pipe(int *files)
{
	if(!files)
		return -EINVAL;
	struct inode *inode = vfs_inode_create();
	inode->uid = current_process->effective_uid;
	inode->gid = current_process->effective_gid;
	inode->mode = S_IFIFO | 0x1FF;
	inode->count = 1;

	struct pipe *pipe = fs_pipe_create();
	inode->devdata = pipe;
	inode->kdev = &__pipe_kdev;

	struct file *rf = file_create(inode, 0, _FREAD);
	struct file *wf = file_create(inode, 0, _FREAD | _FWRITE);
	int read = file_add_filedes(rf, 0);
	int write = file_add_filedes(wf, 0);
	files[0] = read;
	files[1] = write;
	file_put(wf);
	file_put(rf);
	vfs_icache_put(inode);
	return 0;
}

