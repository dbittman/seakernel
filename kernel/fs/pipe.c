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
#include <sea/string.h>
#include <sea/tm/blocking.h>
pipe_t *fs_pipe_create (void)
{
	pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
	pipe->length = PIPE_SIZE;
	pipe->buffer = (char *)kmalloc(PIPE_SIZE);
	mutex_create(&pipe->lock, 0);
	blocklist_create(&pipe->read_blocked, 0);
	blocklist_create(&pipe->write_blocked, 0);
	return pipe;
}

static struct inode *create_anon_pipe (void)
{
	struct inode *node;
	/* create a 'fake' inode */
	node = vfs_inode_create();
	node->flags |= INODE_NOLRU;
	node->uid = current_process->effective_uid;
	node->gid = current_process->effective_gid;
	node->mode = S_IFIFO | 0x1FF;
	node->count=2;
	
	pipe_t *pipe = fs_pipe_create();
	pipe->count=2;
	pipe->wrcount=1;
	node->pipe = pipe;
	return node;
}

int sys_pipe(int *files)
{
	if(!files) return -EINVAL;
	struct file *f;
	struct inode *inode = create_anon_pipe();
	/* this is the reading descriptor */
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flags = _FREAD;
	f->pos=0;
	f->count=1;
	int read = fs_add_file_pointer(current_process, f);
	/* this is the writing descriptor */
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flags = _FREAD | _FWRITE;
	f->count=1;
	f->pos=0;
	int write = fs_add_file_pointer(current_process, f);
	files[0]=read;
	files[1]=write;
	fs_fput(current_process, read, 0);
	fs_fput(current_process, write, 0);
	return 0;
}

void fs_pipe_free(struct inode *i)
{
	if(!i || !i->pipe) return;
	kfree((void *)i->pipe->buffer);
	mutex_destroy(&i->pipe->lock);
	blocklist_destroy(&i->pipe->read_blocked);
	blocklist_destroy(&i->pipe->write_blocked);
	kfree(i->pipe);
	i->pipe=0;
}

static bool __release_lock(void *m)
{
	mutex_release((mutex_t *)m);
	return true;
}

int fs_pipe_read(struct inode *ino, int flags, char *buffer, size_t length)
{
	if(!ino || !buffer)
		return -EINVAL;
	pipe_t *pipe = ino->pipe;
	if(!pipe)
		return -EINVAL;
	size_t len = length;
	int ret=0;
	if((flags & _FNONBLOCK) && !pipe->pending)
		return -EAGAIN;
	/* should we even try reading? (empty pipe with no writing processes=no) */
	if(!pipe->pending && pipe->count <= 1)
		return 0;
	/* block until we have stuff to read */
	mutex_acquire(&pipe->lock);
	while(!pipe->pending && (pipe->count > 1 && pipe->wrcount>0)) {
		/* we need to block, but also release the lock. Disable interrupts
		 * so we don't schedule before we want to */
		int r = tm_thread_block_confirm(&pipe->read_blocked,
				THREADSTATE_INTERRUPTIBLE, __release_lock, &pipe->lock);
		switch(r) {
			case -ERESTART:
				return -ERESTART;
			case -EINTR:
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

	tm_blocklist_wakeall(&pipe->write_blocked);
	tm_blocklist_wakeall(&pipe->read_blocked);
	
	mutex_release(&pipe->lock);
	return ret;
}

int fs_pipe_write(struct inode *ino, int flags, char *initialbuffer, size_t totallength)
{
	if(!ino || !initialbuffer)
		return -EINVAL;
	pipe_t *pipe = ino->pipe;
	if(!pipe)
		return -EINVAL;
	/* allow for partial writes of the system page size. Thus, we wont
	 * have a process freeze because it tries to fill up the pipe in one
	 * shot. */
	char *buffer = initialbuffer;
	size_t length;
	size_t remain = totallength;
	mutex_acquire(&pipe->lock);
	if((flags & _FNONBLOCK) && pipe->pending + totallength > PIPE_SIZE) {
		mutex_release(&pipe->lock);
		return -EAGAIN;
	}
	while(remain) {
		length = PAGE_SIZE;
		if(length > remain)
			length = remain;
		/* we're writing to a pipe with no reading process! */
		if((pipe->count - pipe->wrcount) == 0) {
			mutex_release(&pipe->lock);
			tm_signal_send_thread(current_thread, SIGPIPE);
			return -EPIPE;
		}
		/* IO block until we can write to it */
		while((pipe->pending+length)>=PIPE_SIZE) {
			tm_blocklist_wakeall(&pipe->read_blocked);
			int r = tm_thread_block_confirm(&pipe->write_blocked,
					THREADSTATE_INTERRUPTIBLE, __release_lock, &pipe->lock);
			switch(r) {
				case -ERESTART:
					return -ERESTART;
				case -EINTR:
					return -EINTR;
			}
			mutex_acquire(&pipe->lock);
		}
		for(unsigned i=0;i<length;i++) {
			pipe->buffer[pipe->write_pos % PIPE_SIZE] = buffer[i];
			pipe->write_pos++;
		}
		pipe->length = ino->length;
		pipe->pending += length;
		/* now, unblock the tasks */
		tm_blocklist_wakeall(&pipe->read_blocked);
		tm_blocklist_wakeall(&pipe->write_blocked);

		remain -= length;
		buffer += length;
	}
	mutex_release(&pipe->lock);
	return totallength;
}

int fs_pipe_select(struct inode *in, int rw)
{
	if(rw != READ)
		return 1;
	pipe_t *pipe = in->pipe;
	if(!pipe) return 1;
	if(!pipe->pending && (pipe->count > 1 && pipe->wrcount>0))
		return 0;
	return 1;
}

