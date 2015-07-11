#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/fs/pipe.h>
#include <sea/tm/process.h>
#include <sea/fs/file.h>
#include <sea/cpu/interrupt.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>

pipe_t *fs_pipe_create (void)
{
	pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
	pipe->length = PIPE_SIZE;
	pipe->buffer = (char *)kmalloc(PIPE_SIZE+1);
	pipe->lock = mutex_create(0, 0);
	pipe->read_blocked = ll_create(0);
	pipe->write_blocked = ll_create(0);
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
	mutex_destroy(i->pipe->lock);
	ll_destroy(i->pipe->read_blocked);
	ll_destroy(i->pipe->write_blocked);
	kfree(i->pipe);
	i->pipe=0;
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
	size_t count=0;
	if((flags & _FNONBLOCK) && !pipe->pending)
		return -EAGAIN;
	/* should we even try reading? (empty pipe with no writing processes=no) */
	if(!pipe->pending && pipe->count <= 1 && pipe->type != PIPE_NAMED)
		return count;
	/* block until we have stuff to read */
	mutex_acquire(pipe->lock);
	while(!pipe->pending && (pipe->count > 1 && pipe->type != PIPE_NAMED 
			&& pipe->wrcount>0)) {
		/* we need to block, but also release the lock. Disable interrupts
		 * so we don't schedule before we want to */
		/* TODO: switch this to preempt disable. Switch all calls to this to that if possible */
		int old = cpu_interrupt_set(0);
		tm_thread_add_to_blocklist(current_thread, pipe->read_blocked);
		mutex_release(pipe->lock);
		tm_schedule();
		cpu_interrupt_set(old);
		if(tm_thread_got_signal(current_thread))
			return -EINTR;
		mutex_acquire(pipe->lock);
	}
	ret = pipe->pending > len ? len : pipe->pending;
	/* note: this is a quick implementation of line-buffering that should
	 * work for most cases. There is currently no way to disable line
	 * buffering in pipes, but I don't care, because there shouldn't be a
	 * reason to. */
	char *nl = strchr((char *)pipe->buffer+pipe->read_pos, '\n');
	if(nl && (nl-(pipe->buffer+pipe->read_pos)) < ret)
		ret = (nl-(pipe->buffer+pipe->read_pos))+1;
	memcpy((void *)(buffer + count), (void *)(pipe->buffer + pipe->read_pos), ret);
	memcpy((void *)pipe->buffer, (void *)(pipe->buffer + pipe->read_pos + ret), 
		PIPE_SIZE - (pipe->read_pos + ret));
	if(ret > 0) {
		pipe->pending -= ret;
		pipe->write_pos -= ret;
		len -= ret;
		count+=ret;
	}
	tm_blocklist_wakeall(pipe->write_blocked);
	tm_blocklist_wakeall(pipe->read_blocked);
	mutex_release(pipe->lock);
	return count;
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
	mutex_acquire(pipe->lock);
	if((flags & _FNONBLOCK) && pipe->write_pos + totallength > PIPE_SIZE) {
		mutex_release(pipe->lock);
		return -EAGAIN;
	}
	while(remain) {
		length = PAGE_SIZE;
		if(length > remain)
			length = remain;
		/* we're writing to a pipe with no reading process! */
		if((pipe->count - pipe->wrcount) == 0 && pipe->type != PIPE_NAMED) {
			mutex_release(pipe->lock);
			/* TODO: we probably shouldn't ever set this directly... remove all code that does this */
			tm_signal_send_thread(current_thread, SIGPIPE);
			return -EPIPE;
		}
		/* IO block until we can write to it */
		while((pipe->write_pos+length)>=PIPE_SIZE) {
			int old = cpu_interrupt_set(0);
			tm_blocklist_wakeall(pipe->read_blocked);
			tm_thread_add_to_blocklist(current_thread, pipe->write_blocked);
			mutex_release(pipe->lock);
			tm_schedule();
			cpu_interrupt_set(old);
			if(tm_thread_got_signal(current_thread))
				return -EINTR;
			mutex_acquire(pipe->lock);
		}
		
		memcpy((void *)(pipe->buffer + pipe->write_pos), buffer, length);
		pipe->length = ino->length;
		pipe->write_pos += length;
		pipe->pending += length;
		/* now, unblock the tasks */
		tm_blocklist_wakeall(pipe->read_blocked);
		tm_blocklist_wakeall(pipe->write_blocked);

		remain -= length;
		buffer += length;
	}
	mutex_release(pipe->lock);
	return totallength;
}

int fs_pipe_select(struct inode *in, int rw)
{
	if(rw != READ)
		return 1;
	pipe_t *pipe = in->pipe;
	if(!pipe) return 1;
	if(!pipe->pending && (pipe->count > 1 && pipe->type != PIPE_NAMED 
			&& pipe->wrcount>0))
		return 0;
	return 1;
}

