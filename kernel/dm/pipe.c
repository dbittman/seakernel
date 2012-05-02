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
	struct inode *root_nodes;
	root_nodes = (struct inode *)kmalloc(sizeof(struct inode));
	root_nodes->uid = current_task->uid;
	root_nodes->gid = current_task->gid;
	root_nodes->mode = S_IFIFO | 0x1FF;
	create_mutex(&root_nodes->lock);
	
	pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
	pipe->length = PIPE_SIZE;
	pipe->buffer = (char *)kmalloc(PIPE_SIZE+1);
	pipe->count=2;
	pipe->wrcount=1;
	pipe->lock = create_mutex(0);
	root_nodes->pipe = pipe;
	return root_nodes;
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
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flag = _FREAD;
	f->pos=0;
	f->count=1;
	int read = add_file_pointer((task_t *)current_task, f);
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flag = _FREAD | _FWRITE;
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
	i->pipe->lock->pid=-1;
	destroy_mutex(i->pipe->lock);
	kfree(i->pipe);
}

__attribute__((optimize("O0"))) int read_pipe(struct inode *ino, char *buffer, unsigned length)
{
	if(!ino || !buffer)
		return -EINVAL;
	pipe_t *pipe = ino->pipe;
	if(!pipe){
		kprintf("Tried to pipe with a non-pipe!\n");
		return -EINVAL;
	}
	unsigned len = length;
	int ret=0;
	int count=0;
	if(!pipe->pending && pipe->count <= 1 && pipe->type != PIPE_NAMED)
		return count;
	while(!pipe->pending && (pipe->count > 1 && pipe->type != PIPE_NAMED && pipe->wrcount>0)) {
		__super_sti();
		force_schedule();
		if(current_task->sigd)
			return -EINTR;
	}
	mutex_on(pipe->lock);
	ret = pipe->pending > len ? len : pipe->pending;
	memcpy(buffer + count, (void *)(pipe->buffer + pipe->read_pos), ret);
	if(ret > 0) {
		pipe->read_pos += ret;
		pipe->pending -= ret;
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
	if(!pipe) {
		kprintf("Tried to pipe with a non-pipe!\n");
		return -EINVAL;
	}
	mutex_on(pipe->lock);
	if((pipe->write_pos+length)>=PIPE_SIZE)
	{
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
