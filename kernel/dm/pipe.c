#include <kernel.h>
#include <dev.h>
#include <fs.h>
#include <pipe.h>

static struct inode *create_pipe(char *name, unsigned mode)
{
	if(!name) return 0;
	struct inode *node;
	node = (struct inode *)cget_idir(name, 0, mode&0xFFF);
	if(!node)
		return 0;
	node->mode = S_IFIFO | (mode&0xFFF);
	pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
	pipe->length = PIPE_SIZE;
	pipe->buffer = (char *)kmalloc(PIPE_SIZE+1);
	pipe->type = PIPE_NAMED;
	node->pipe = pipe;
	pipe->lock = create_mutex(0);
	node->start = (unsigned)pipe->buffer;
	return node;
}

static struct inode *create_anon_pipe()
{
	struct inode *node;
	/* create a 'fake' inode */
	node = (struct inode *)kmalloc(sizeof(struct inode));
	_strcpy(node->name, "~pipe~");
	node->uid = current_task->uid;
	node->gid = current_task->gid;
	node->mode = S_IFIFO | 0x1FF;
	node->count=2;
	node->f_count=2;
	create_mutex(&node->lock);
	
	pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
	pipe->length = PIPE_SIZE;
	pipe->buffer = (char *)kmalloc(PIPE_SIZE+1);
	pipe->count=2;
	pipe->wrcount=1;
	pipe->lock = create_mutex(0);
	node->pipe = pipe;
	return node;
}

int sys_mkfifo(char *path, unsigned mode)
{
	if(!create_pipe(path, mode))
		return -EINVAL;
	return 0;
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
	int read = add_file_pointer((task_t *)current_task, f);
	/* this is the writing descriptor */
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flags = _FREAD | _FWRITE;
	f->count=1;
	f->pos=0;
	int write = add_file_pointer((task_t *)current_task, f);
	files[0]=read;
	files[1]=write;
	return 0;
}

void free_pipe(struct inode *i)
{
	if(!i || !i->pipe) return;
	kfree((void *)i->pipe->buffer);
	destroy_mutex(i->pipe->lock);
	kfree(i->pipe);
}

__attribute__((optimize("O0"))) int read_pipe(struct inode *ino, char *buffer, unsigned length)
{
	if(!ino || !buffer)
		return -EINVAL;
	pipe_t *pipe = ino->pipe;
	if(!pipe)
		return -EINVAL;
	unsigned len = length;
	int ret=0;
	int count=0;
	if(!pipe->pending && pipe->count <= 1 && pipe->type != PIPE_NAMED)
		return count;
	while(!pipe->pending && (pipe->count > 1 && pipe->type != PIPE_NAMED 
			&& pipe->wrcount>0)) {
		__super_sti();
		force_schedule();
		if(current_task->sigd)
			return -EINTR;
	}
	mutex_on(pipe->lock);
	ret = pipe->pending > len ? len : pipe->pending;
	memcpy((void *)(buffer + count), (void *)(pipe->buffer + pipe->read_pos), ret);
	memcpy((void *)pipe->buffer, (void *)(pipe->buffer + pipe->read_pos + ret), 
		PIPE_SIZE - (pipe->read_pos + ret));
	if(ret > 0) {
		pipe->pending -= ret;
		pipe->write_pos -= ret;
		len -= ret;
		count+=ret;
	} else {
		mutex_off(pipe->lock);
		return count;
	}
	mutex_off(pipe->lock);
	return count;
}

__attribute__((optimize("O0"))) int write_pipe(struct inode *ino, char *buffer, unsigned length)
{
	if(!ino || !buffer)
		return -EINVAL;
	pipe_t *pipe = ino->pipe;
	if(!pipe)
		return -EINVAL;
	mutex_on(pipe->lock);
	
	while((pipe->write_pos+length)>=PIPE_SIZE && (pipe->count > 1 
			&& pipe->type != PIPE_NAMED)) {
		mutex_off(pipe->lock);
		wait_flag_except((unsigned *)&pipe->write_pos, pipe->write_pos);
		if(current_task->sigd)
			return -EINTR;
		mutex_on(pipe->lock);
	}
	if(pipe->count <= 1) {
		mutex_off(pipe->lock);
		return -EPIPE;
	}
	if((pipe->write_pos+length)>=PIPE_SIZE)
	{
		printk(1, "[pipe]: warning - task %d failed to block for writing to pipe\n"
			, current_task->pid);
		mutex_off(pipe->lock);
		return -EPIPE;
	}
	memcpy((void *)(pipe->buffer + pipe->write_pos), buffer, length);
	pipe->length = ino->len;
	pipe->write_pos += length;
	pipe->pending += length;
	mutex_off(pipe->lock);
	return length;
}

int pipedev_select(struct inode *in, int rw)
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
